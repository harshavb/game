//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//


#include "OptionsSubKeyboard.h"
#include "EngineInterface.h"
#include "VControlsListPanel.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ScrollBar.h"

#include "vgui/Cursor.h"
#include "vgui/ISurface.h"
#include "tier1/KeyValues.h"
#include "vgui/KeyCode.h"
#include "vgui/IInput.h"

#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include "IGameUIFuncs.h"
#include "vstdlib/IKeyValuesSystem.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubKeyboard::COptionsSubKeyboard(vgui::Panel *parent) : PropertyPage(parent, "OptionsSubKeyboard" )
{
    SetSize(0, 2);

    // create the key bindings list
    m_pKeyBindList = new VControlsListPanel(this, "listpanel_keybindlist");
	// Parse default descriptions
	ParseActionDescriptions();
	
	m_pSetBindingButton = new Button(this, "ChangeKeyButton", "");
	m_pClearBindingButton = new Button(this, "ClearKeyButton", "");

	LoadControlSettings("resource/OptionsSubKeyboard.res");

	m_pSetBindingButton->SetEnabled(false);
	m_pClearBindingButton->SetEnabled(false);
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: reloads current keybinding
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnResetData()
{
	// Populate list based on current settings
	FillInCurrentBindings();
	if ( IsVisible() )
	{
		m_pKeyBindList->SetSelectedItem(0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: binds double-clicking or hitting enter in the keybind list to changing the key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnKeyCodeTyped(vgui::KeyCode code)
{
	if (code == KEY_ENTER)
	{
		OnCommand("ChangeKey");
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: command handler
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnCommand( const char *command )
{
	if ( !stricmp( command, "Defaults" )  )
	{
		// open a box asking if we want to restore defaults
		QueryBox *box = new QueryBox("#GameUI_KeyboardSettings", "#GameUI_KeyboardSettingsText");
		box->AddActionSignalTarget(this);
		box->SetOKCommand(new KeyValues("Command", "command", "DefaultsOK"));
		box->DoModal();
	}
	else if ( !stricmp(command, "DefaultsOK"))
	{
		FillInDefaultBindings();
		m_pKeyBindList->RequestFocus();
	}
	else if ( !m_pKeyBindList->IsCapturing() && !stricmp( command, "ChangeKey" ) )
	{
		m_pKeyBindList->StartCaptureMode(dc_blank);
	}
	else if ( !m_pKeyBindList->IsCapturing() && !stricmp( command, "ClearKey" ) )
	{
		OnKeyCodePressed( KEY_DELETE ); // <<< PC only code, no need for joystick management
        m_pKeyBindList->RequestFocus();
	}
	else if ( !stricmp(command, "Advanced") )
	{
		OpenKeyboardAdvancedDialog();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ParseActionDescriptions( void )
{
	char szBinding[256];
	char szDescription[256];

	KeyValues *item;

	// Load the default keys list
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( "scripts/kb_act.lst", NULL, buf ) )
		return;

	const char *data = (const char*)buf.Base();

	int sectionIndex = 0;
	char token[512];
	while ( 1 )
	{
        data = engine->ParseFile(data, token, sizeof(token));
		// Done.
		if ( strlen( token ) <= 0 )  
			break;

		Q_strncpy( szBinding, token, sizeof( szBinding ) );

		data = engine->ParseFile(data, token, sizeof(token));
		if ( strlen(token) <= 0 )
		{
			break;
		}

		Q_strncpy(szDescription, token, sizeof( szDescription ) );

		// Skip '======' rows
		if ( szDescription[ 0 ] != '=' )
		{
			// Flag as special header row if binding is "blank"
			if (!stricmp(szBinding, "blank"))
			{
				// add header item
				int nColumn1 = 286;
				int nColumn2 = 128;
				if ( IsProportional() )
				{
					nColumn1 = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), nColumn1 );
					nColumn2 = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), nColumn2 );
				}
				m_pKeyBindList->AddSection(++sectionIndex, szDescription);
				m_pKeyBindList->AddColumnToSection(sectionIndex, "Action", szDescription, SectionedListPanel::COLUMN_BRIGHT, nColumn1 );
				m_pKeyBindList->AddColumnToSection(sectionIndex, "Key", "#GameUI_KeyButton", SectionedListPanel::COLUMN_BRIGHT, nColumn2 );
			}
			else
			{
				// Create a new: blank item
				item = new KeyValues( "Item" );
				
				item->SetString("Action", szDescription);
				item->SetString("Binding", szBinding);
				item->SetString("Key", "");

				m_pKeyBindList->AddItem(sectionIndex, item);
				item->deleteThis();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Search current data set for item which has the specified binding string
// Input  : *binding - string to find
// Output : KeyValues or NULL on failure
//-----------------------------------------------------------------------------
KeyValues *COptionsSubKeyboard::GetItemForBinding( const char *binding )
{
	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString("Binding");

	// Loop through all items
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		KeyValues *bindingItem = item->FindKey(bindingSymbol);
		const char *bindString = bindingItem->GetString();

		// Check the "Binding" key
		if (!stricmp(bindString, binding))
			return item;
	}
	// Didn't find it
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Bind the specified keyname to the specified item
// Input  : *item - Item to which to add the key
//			*keyname - The key to be added
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::AddBinding( KeyValues *item, const char *keyname )
{
	// See if it's already there as a binding
	if ( !stricmp( item->GetString( "Key", "" ), keyname ) )
		return;

	// Make sure it doesn't live anywhere
	RemoveKeyFromBindItems( item, keyname );

	const char *binding = item->GetString( "Binding", "" );

	// Loop through all the key bindings and set all entries that have
	// the same binding. This allows us to have multiple entries pointing 
	// to the same binding.
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *curitem = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !curitem )
			continue;

		const char *curbinding = curitem->GetString( "Binding", "" );

		if (!stricmp(curbinding, binding))
		{
			curitem->SetString( "Key", keyname );
            BindKey(keyname, binding);
			m_pKeyBindList->InvalidateItem(i);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remove all keys from list
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ClearBindItems( void )
{
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		item->SetString( "Key", "" );

		m_pKeyBindList->InvalidateItem(i);
	}

	m_pKeyBindList->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Remove all instances of the specified key from bindings
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::RemoveKeyFromBindItems( KeyValues *org_item, const char *key )
{
	Assert( key && key[ 0 ] );
	if ( !key || !key[ 0 ] )
		return;

	int len = Q_strlen( key );
	char *pszKey = new char[len + 1];

	if ( !pszKey )
		return;

	Q_memcpy( pszKey, key, len+1 );

	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		// If it's bound to the primary: then remove it
        const char *curKey = item->GetString("Key", "");
		if ( !stricmp( pszKey,  curKey ) )
		{
			bool bClearEntry = true;

			if ( org_item )
			{
				// Only clear it out if the actual binding isn't the same. This allows
				// us to have the same key bound to multiple entries in the keybinding list
				// if they point to the same command.
				const char *org_binding = org_item->GetString( "Binding", "" );
				const char *binding = item->GetString( "Binding", "" );
				if ( !stricmp( org_binding, binding ) )
				{
					bClearEntry = false;
				}
			}

			if ( bClearEntry )
            {
                UnbindKey(key);
				item->SetString( "Key", "" );
				m_pKeyBindList->InvalidateItem(i);
			}
		}
	}

	delete [] pszKey;

	// Make sure the display is up to date
	m_pKeyBindList->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Ask the engine for all bindings and set up the list
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::FillInCurrentBindings( void )
{
	// Clear any current settings
	ClearBindItems();

	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		ButtonCode_t bc = static_cast<ButtonCode_t>(i);

		// Look up binding
		const char *binding = gameuifuncs->GetBindingForButtonCode( bc );
		if ( !binding )
			continue;

		// See if there is an item for this one?
		KeyValues *item = GetItemForBinding( binding );
		if ( item )
		{
			// Bind it by name
            AddBinding(item, g_pInputSystem->ButtonCodeToString(bc));
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to bind a key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::BindKey( const char *key, const char *binding )
{
    engine->ClientCmd_Unrestricted(CFmtStr("bind \"%s\" \"%s\"\n", key, binding));
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to unbind a key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::UnbindKey( const char *key )
{
    engine->ClientCmd_Unrestricted(CFmtStr("unbind \"%s\"\n", key));
}

//-----------------------------------------------------------------------------
// Purpose: Read in defaults from game's default config file and populate list 
//			using those defaults
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::FillInDefaultBindings( void )
{
	FileHandle_t fh = g_pFullFileSystem->Open( "cfg/config_default.cfg", "rb" );
	if (fh == FILESYSTEM_INVALID_HANDLE)
		return;

	engine->ClientCmd_Unrestricted( "unbindall\n" );

	int size = g_pFullFileSystem->Size(fh) + 1;
	CUtlBuffer buf( 0, size, CUtlBuffer::TEXT_BUFFER );
	g_pFullFileSystem->Read( buf.Base(), size, fh );
	g_pFullFileSystem->Close(fh);

	// NULL terminate!
	((char*)buf.Base())[ size - 1 ] = '\0';

	ClearBindItems();

	const char *data = (const char*)buf.Base();

	// loop through all the bindings
	while ( data != NULL )
	{
        char cmd[64];
        data = engine->ParseFile(data, cmd, sizeof(cmd));
        if (strlen(cmd) <= 0)
            break;

        if (!stricmp(cmd, "bind"))
        {
            char szKeyName[256];
            data = engine->ParseFile(data, szKeyName, sizeof(szKeyName));
            if (strlen(szKeyName) <= 0)
                break; 

            char szBinding[256];
            data = engine->ParseFile(data, szBinding, sizeof(szBinding));
            if (strlen(szKeyName) <= 0)
                break; 

            KeyValues *item = GetItemForBinding(szBinding);
            if (item)
            {
                AddBinding(item, szKeyName);
            }
        }
	}

	// Make sure console and escape key are always valid
    KeyValues *item = GetItemForBinding( "toggleconsole" );
    if ( item )
    {
        AddBinding( item, "`" );
    }
    item = GetItemForBinding( "cancelselect" );
    if ( item )
    {
        AddBinding( item, "ESCAPE" );
    }
}

//-----------------------------------------------------------------------------
// Purpose: User clicked on item: remember where last active row/column was
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ItemSelected(int itemID)
{
	m_pKeyBindList->SetItemOfInterest(itemID);

	if (m_pKeyBindList->IsItemIDValid(itemID))
	{
		// find the details, see if we should be enabled/clear/whatever
		m_pSetBindingButton->SetEnabled(true);

		KeyValues *kv = m_pKeyBindList->GetItemData(itemID);
		if (kv)
		{
			const char *key = kv->GetString("Key", NULL);
			if (key && *key)
			{
				m_pClearBindingButton->SetEnabled(true);
			}
			else
			{
				m_pClearBindingButton->SetEnabled(false);
			}

			if (kv->GetInt("Header"))
			{
				m_pSetBindingButton->SetEnabled(false);
			}
		}
	}
	else
	{
		m_pSetBindingButton->SetEnabled(false);
		m_pClearBindingButton->SetEnabled(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the capture has finished
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::Finish( ButtonCode_t code )
{
	int r = m_pKeyBindList->GetItemOfInterest();

	// Retrieve clicked row and column
	m_pKeyBindList->EndCaptureMode( dc_arrow );

	// Find item for this row
	KeyValues *item = m_pKeyBindList->GetItemData(r);
	if ( item )
	{
		// Handle keys: but never rebind the escape key
		// Esc just exits bind mode silently
		if ( code != BUTTON_CODE_NONE && code != KEY_ESCAPE && code != BUTTON_CODE_INVALID )
		{
			// Bind the named key
			AddBinding( item, g_pInputSystem->ButtonCodeToString( code ) );
		}

		m_pKeyBindList->InvalidateItem(r);
	}

	m_pSetBindingButton->SetEnabled(true);
	m_pClearBindingButton->SetEnabled(true);
}

//-----------------------------------------------------------------------------
// Purpose: Scans for captured key presses
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnThink()
{
	BaseClass::OnThink();

	if ( m_pKeyBindList->IsCapturing() )
	{
		ButtonCode_t code = BUTTON_CODE_INVALID;
		if ( engine->CheckDoneKeyTrapping( code ) )
		{
			Finish( code );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check for enter key and go into keybinding mode if it was pressed
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnKeyCodePressed(vgui::KeyCode code)
{
	// Enter key pressed and not already trapping next key/button press
	if ( !m_pKeyBindList->IsCapturing() )
	{
		// Grab which item was set as interesting
		int r = m_pKeyBindList->GetItemOfInterest();

		// Check that it's visible
		int x, y, w, h;
		bool visible = m_pKeyBindList->GetCellBounds(r, 1, x, y, w, h);
		if (visible)
		{
			if ( KEY_DELETE == code )
			{
				// find the current binding and remove it
				KeyValues *kv = m_pKeyBindList->GetItemData(r);

				const char *key = kv->GetString("Key", NULL);
				if (key && *key)
				{
					RemoveKeyFromBindItems(NULL, key);
				}

				m_pClearBindingButton->SetEnabled(false);
				m_pKeyBindList->InvalidateItem(r);

				// message handled, don't pass on
				return;
			}
		}
	}

	// Allow base class to process message instead
	BaseClass::OnKeyCodePressed( code );
}


//-----------------------------------------------------------------------------
// Purpose: advanced keyboard settings dialog
//-----------------------------------------------------------------------------
COptionsSubKeyboardAdvancedDlg::COptionsSubKeyboardAdvancedDlg(vgui::VPANEL hParent) : BaseClass(NULL, NULL),
    m_cvarConEnable("con_enable"), m_cvarFastSwitch("hud_fastswitch", true)
{
    // parent is ignored, since we want look like we're steal focus from the parent (we'll become modal below)
    SetTitle("#GameUI_KeyboardAdvanced_Title", true);
    SetSize(280, 140);
    LoadControlSettings("resource/OptionsSubKeyboardAdvancedDlg.res");
    MoveToCenterOfScreen();
    SetSizeable(false);
    SetDeleteSelfOnClose(true);
}

void COptionsSubKeyboardAdvancedDlg::Activate()
{
	BaseClass::Activate();

	input()->SetAppModalSurface(GetVPanel());

	// reset the data
	if ( m_cvarConEnable.IsValid() )
	{
        SetControlInt("ConsoleCheck", m_cvarConEnable.GetInt() ? 1 : 0);
	}

	if ( m_cvarFastSwitch.IsValid() )
	{
        SetControlInt("FastSwitchCheck", m_cvarFastSwitch.GetInt() ? 1 : 0);
	}
}

void COptionsSubKeyboardAdvancedDlg::OnApplyData()
{
    m_cvarConEnable.SetValue(GetControlInt("ConsoleCheck", 0));

	m_cvarFastSwitch.SetValue(GetControlInt("FastSwitchCheck", 0));
}

void COptionsSubKeyboardAdvancedDlg::OnCommand(const char *command)
{
	if ( !stricmp(command, "OK") )
	{
		OnApplyData();
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void COptionsSubKeyboardAdvancedDlg::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		Close();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Open advanced keyboard options
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OpenKeyboardAdvancedDialog()
{
	if (!m_OptionsSubKeyboardAdvancedDlg.Get())
	{
		m_OptionsSubKeyboardAdvancedDlg = new COptionsSubKeyboardAdvancedDlg(GetVParent());
	}
	m_OptionsSubKeyboardAdvancedDlg->Activate();
}