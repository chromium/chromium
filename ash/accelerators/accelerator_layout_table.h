// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece_forward.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// non-ash accelerator action Id. Contains browser action ids and ambient action
// ids.
enum NonConfigurableActions {
  // Browser action ids:
  kBrowserCloseTab,
  kBrowserCloseWindow,
  kBrowserSelectLastTab,
  kBrowserOpenFile,
  kBrowserNewIncognitoWindow,
  kBrowserNewTab,
  kBrowserNewWindow,
  kBrowserRestoreTab,
  kBrowserTabSearch,
  kBrowserClearBrowsingData,
  kBrowserCloseFindOrStop,
  kBrowserFocusBookmarks,
  kBrowserBack,
  kBrowserForward,
  kBrowserFind,
  kBrowserFindNext,
  kBrowserFindPrevious,
  kBrowserHome,
  kBrowserShowDownloads,
  kBrowserShowHistory,
  kBrowserFocusSearch,
  kBrowserFocusMenuBar,
  kBrowserPrint,
  kBrowserReload,
  kBrowserReloadBypassingCache,
  kBrowserZoomNormal,
  kBrowserBookmarkAllTabs,
  kBrowserSavePage,
  kBrowserBookmarkThisTab,
  kBrowserShowAppMenu,
  kBrowserShowBookmarkManager,
  kBrowserDevToolsConsole,
  kBrowserDebToolsInspect,
  kBrowserDevTools,
  kBrowserShowBookmarkBar,
  kBrowserViewSource,
  kBrowserZoomPlus,
  kBrowserZoomMinus,
  kBrowserFocusLocation,
  kBrowserFocusToolbar,
  kBrowserFocusInactivePopupForAccessibility,
  kBrowserSelectTabByIndex,
};

// Contains details for UI styling of an accelerator.
struct ASH_EXPORT AcceleratorLayoutDetails {
  // The accelerator action id associated for a source. Concat `source` and
  // `action_id` to get a unique identifier for an accelerator action.
  uint32_t action_id;

  // Category of the accelerator.
  mojom::AcceleratorCategory category;

  // Subcategory of the accelerator.
  mojom::AcceleratorSubcategory sub_category;

  // True if the accelerator cannot be modified through customization.
  // False if the accelerator can be modified through customization.
  bool locked;

  // The layout style of the accelerator, this provides additional context
  // on how to accelerator should be represented in the UI.
  mojom::AcceleratorLayoutStyle layout_style;

  // The source of which the accelerator is from.
  mojom::AcceleratorSource source;
};

// A map between browser action id and accelerator description ID.
ASH_EXPORT constexpr auto kBrowserActionToStringIdMap = base::MakeFixedFlatMap<
    NonConfigurableActions,
    int>({
    {NonConfigurableActions::kBrowserCloseTab,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_CLOSE_TAB},
    {NonConfigurableActions::kBrowserCloseWindow,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_CLOSE_WINDOW},
    {NonConfigurableActions::kBrowserSelectLastTab,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_SELECT_LAST_TAB},
    {NonConfigurableActions::kBrowserOpenFile,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_OPEN_FILE},
    {NonConfigurableActions::kBrowserNewIncognitoWindow,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_NEW_INCOGNITO_WINDOW},
    {NonConfigurableActions::kBrowserNewTab,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_NEW_TAB},
    {NonConfigurableActions::kBrowserNewWindow,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_NEW_WINDOW},
    {NonConfigurableActions::kBrowserRestoreTab,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_RESTORE_TAB},
    {NonConfigurableActions::kBrowserTabSearch,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_TAB_SEARCH},
    {NonConfigurableActions::kBrowserClearBrowsingData,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_CLEAR_BROWSING_DATA},
    {NonConfigurableActions::kBrowserCloseFindOrStop,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_CLOSE_FIND_OR_STOP},
    {NonConfigurableActions::kBrowserFocusBookmarks,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FOCUS_BOOKMARKS},
    {NonConfigurableActions::kBrowserBack,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_BACK},
    {NonConfigurableActions::kBrowserForward,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FORWARD},
    {NonConfigurableActions::kBrowserFind,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FIND},
    {NonConfigurableActions::kBrowserFindNext,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FIND_NEXT},
    {NonConfigurableActions::kBrowserFindPrevious,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FIND_PREVIOUS},
    {NonConfigurableActions::kBrowserHome,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_HOME},
    {NonConfigurableActions::kBrowserShowDownloads,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_SHOW_DOWNLOADS},
    {NonConfigurableActions::kBrowserShowHistory,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_SHOW_HISTORY},
    {NonConfigurableActions::kBrowserFocusSearch,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FOCUS_SEARCH},
    {NonConfigurableActions::kBrowserFocusMenuBar,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FOCUS_MENU_BAR},
    {NonConfigurableActions::kBrowserPrint,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_PRINT},
    {NonConfigurableActions::kBrowserReload,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_RELOAD},
    {NonConfigurableActions::kBrowserReloadBypassingCache,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_RELOAD_BYPASSING_CACHE},
    {NonConfigurableActions::kBrowserZoomNormal,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_ZOOM_NORMAL},
    {NonConfigurableActions::kBrowserBookmarkAllTabs,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_BOOKMARK_ALL_TABS},
    {NonConfigurableActions::kBrowserSavePage,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_SAVE_PAGE},
    {NonConfigurableActions::kBrowserBookmarkThisTab,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_BOOKMARK_THIS_TAB},
    {NonConfigurableActions::kBrowserShowAppMenu,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_SHOW_APP_MENU},
    {NonConfigurableActions::kBrowserShowBookmarkManager,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_SHOW_BOOKMARK_MANAGER},
    {NonConfigurableActions::kBrowserDevToolsConsole,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_DEV_TOOLS_CONSOLE},
    {NonConfigurableActions::kBrowserDebToolsInspect,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_DEV_TOOLS_INSPECT},
    {NonConfigurableActions::kBrowserDevTools,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_DEV_TOOLS},
    {NonConfigurableActions::kBrowserShowBookmarkBar,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_SHOW_BOOKMARK_BAR},
    {NonConfigurableActions::kBrowserViewSource,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_VIEW_SOURCE},
    {NonConfigurableActions::kBrowserZoomPlus,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_ZOOM_PLUS},
    {NonConfigurableActions::kBrowserZoomMinus,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_ZOOM_MINUS},
    {NonConfigurableActions::kBrowserFocusLocation,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FOCUS_LOCATION},
    {NonConfigurableActions::kBrowserFocusToolbar,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FOCUS_TOOLBAR},
    {NonConfigurableActions::kBrowserFocusInactivePopupForAccessibility,
     IDS_BROWSER_ACCELERATOR_DESCRIPTION_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY},
});

// Represents a replacement for part of a non-configurable accelerator.
// Contains the text to display as well as its type (Modifier, Key, Plain Text)
// which is needed to determine how to display the text in the shortcut
// customization app.
class ASH_EXPORT TextAcceleratorPart : public mojom::TextAcceleratorPart {
 public:
  explicit TextAcceleratorPart(ui::EventFlags modifier);
  explicit TextAcceleratorPart(ui::KeyboardCode key_code);
  TextAcceleratorPart(const TextAcceleratorPart&);
  TextAcceleratorPart& operator=(const TextAcceleratorPart&);
  ~TextAcceleratorPart();
};

// Contains info related to an ambient accelerator. The |message_id| and list
// of |text_accelerator_parts| are used by AcceleratorConfigurationProvider to
// construct arbitrary text with styled keys and modifiers interspersed.
struct ASH_EXPORT AcceleratorTextDetails {
  AcceleratorTextDetails(int message_id,
                         std::vector<TextAcceleratorPart> parts);
  AcceleratorTextDetails(const AcceleratorTextDetails&);
  ~AcceleratorTextDetails();
  int message_id;
  std::vector<TextAcceleratorPart> text_accelerator_parts;
};

using NonConfigurableActionsTextDetailsMap =
    std::map<NonConfigurableActions, AcceleratorTextDetails>;

// A map between ambient action id and accelerator description ID.
// Adding a new ambient accelerator must add a new entry to this map.
ASH_EXPORT constexpr auto kAmbientActionToStringIdMap =
    base::MakeFixedFlatMap<NonConfigurableActions, int>({
        {NonConfigurableActions::kBrowserSelectTabByIndex,
         IDS_TEXT_ACCELERATOR_DESCRIPTION_GO_TO_TAB_IN_RANGE},
    });

const ASH_EXPORT NonConfigurableActionsTextDetailsMap& GetTextDetailsMap();

// A fixed array of accelerator layouts used for categorization and styling of
// accelerator actions. The ordering of the array is important and is used
// 1:1 for displaying shortcuts in the shortcut customization app.
// Adding an accelerator layout in this array will create a new entry in the
// app.
// TODO(jimmyxgong): This is a stub map with stub details, replace with real
// one when categorization is available.
ASH_EXPORT constexpr AcceleratorLayoutDetails kAcceleratorLayouts[] = {
    // Tab & Windows.
    {DESKS_ACTIVATE_DESK_LEFT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_ACTIVATE_DESK_RIGHT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_NEW_DESK, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {WINDOW_CYCLE_SNAP_LEFT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {WINDOW_CYCLE_SNAP_RIGHT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_MAXIMIZED, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {WINDOW_MINIMIZE, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS,
     mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_MOVE_ACTIVE_ITEM_LEFT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_MOVE_ACTIVE_ITEM_RIGHT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_CROSH, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {ROTATE_WINDOW, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {CYCLE_FORWARD_MRU, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TAKE_WINDOW_SCREENSHOT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_FULLSCREEN, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_RESIZE_LOCK_MENU, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_REMOVE_CURRENT_DESK, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {UNPIN, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {NonConfigurableActions::kBrowserSelectTabByIndex,
     mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kText,
     mojom::AcceleratorSource::kAmbient},

    // Page and Web Browser.
    {FOCUS_PREVIOUS_PANE, mojom::AcceleratorCategory::kPageAndWebBrowser,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},

    // System and display settings.
    {SHOW_TASK_MANAGER, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {LAUNCH_LAST_APP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SCALE_UI_DOWN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SCALE_UI_UP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_MIRROR_MODE, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {VOLUME_MUTE, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_DIAGNOSTICS, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_GET_HELP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_FILE_MANAGER, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_SYSTEM_TRAY_BUBBLE,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_APP_LIST, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SUSPEND, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SCALE_UI_RESET, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {ROTATE_SCREEN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_MESSAGE_CENTER_BUBBLE,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SHOW_STYLUS_TOOLS, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_IME_MENU_BUBBLE,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {EXIT, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_FEEDBACK_PAGE, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWAP_PRIMARY_DISPLAY,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_LAST_USED_IME,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_NEXT_IME, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_NEXT_USER, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_PREVIOUS_USER,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {BRIGHTNESS_DOWN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {BRIGHTNESS_UP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {VOLUME_DOWN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {VOLUME_UP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},

    // Text Editing.
    {KEYBOARD_BRIGHTNESS_DOWN, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {KEYBOARD_BRIGHTNESS_UP, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_CLIPBOARD_HISTORY, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SHOW_EMOJI_PICKER, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_CAPS_LOCK, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},

    // Accessibility
    {FOCUS_SHELF, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_SPOKEN_FEEDBACK, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_HIGH_CONTRAST, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_DOCKED_MAGNIFIER, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_FULLSCREEN_MAGNIFIER, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {MAGNIFIER_ZOOM_IN, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {MAGNIFIER_ZOOM_OUT, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
};
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_
