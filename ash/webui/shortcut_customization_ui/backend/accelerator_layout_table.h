// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_LAYOUT_TABLE_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_LAYOUT_TABLE_H_

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/shortcut_customization_ui/backend/text_accelerator_part.h"
#include "base/containers/fixed_flat_set.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

// IMPORTANT -
// If you plan on adding a new accelerator and want it to be displayed in the
// Key Shortcuts app, please follow the instructions below:
//    1.    Determine the correct category and subcategory the accelerator
//          belongs to. You can view the categories at `accelerator_info.mojom`.
//          Reach out to cros-device-enablement@ if you are unsure about which
//          category to use.
//    2.    If you are adding a browser/ambient [1] accelerator, add a new
//          enum to `NonConfigurableActions`. Then add an entry to
//          `GetNonConfigurableActionsMap` in `accelerator_layout_table.cc`.
//    3.    If the new accelerator does not have a layout and will not appear in
//          the Shortcuts app, add it to `kAshAcceleratorsWithoutLayout` and
//          skip step 4 & 5.
//    4.    Add a new entry to `kAcceleratorLayouts` below. The ordering of the
//          accelerators is reflected in the app, so place the accelerator where
//          it would most logically fit.
//    5.    Add a new entry to `acceleratorLayoutMap` in
//          `accelerator_layout_table.cc`.
//
//   [1]: An "ambient" accelerator is a non-modifiable miscellaneous accelerator
//        that may contain a special set of instructions and/or does not
//        necessarily belong as an ash accelerator. Please note that Browser
//        shortcuts are considered "ambient".

namespace ash {

namespace {

// Non-configurable actions required an offset so that ash accelerator action
// enums don't clash. 10000 is safe enough of an offset to ensure no actions
// can ever clash between the two enums.
inline constexpr int kNonConfigurableActionsOffset = 10000;

}  // namespace

// non-ash accelerator action Id. Contains browser action ids and ambient action
// ids.
enum NonConfigurableActions {
  // Browser action ids:
  kBrowserCloseTab = kNonConfigurableActionsOffset,
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
  kBrowserFocusAddressBar,
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
  kBrowserDevToolsInspect,
  kBrowserDevTools,
  kBrowserShowBookmarkBar,
  kBrowserViewSource,
  kBrowserZoomPlus,
  kBrowserZoomMinus,
  kBrowserFocusLocation,
  kBrowserFocusToolbar,
  kBrowserFocusInactivePopupForAccessibility,
  kBrowserSelectTabByIndex,
  kBrowserBottomPage,
  kBrowserTopPage,
  kBrowserNextPane,
  kBrowserAutoComplete,
  kBrowserStopDragTab,
  kBrowserSelectNextTab,
  kBrowserSelectPreviousTab,
  kBrowserPageUp,
  kBrowserPageDown,
  // Ambient action ids:
  kAmbientDragLinkInSameTab,
  kAmbientCycleForwardMRU,
  kAmbientDragLinkInNewTab,
  kAmbientOpenLinkInTab,
  kAmbientOpenLinkInTabBackground,
  kAmbientOpenLinkInWindow,
  kAmbientOpenPageInNewTab,
  kAmbientCycleBackwardMRU,
  kAmbientRightClick,
  kAmbientSaveLinkAsBookmark,
  kAmbientLaunchNumberedApp,
  kAmbientOpenFile,
  kAmbientOpenHighlightedItemOnShelf,
  kAmbientHighlightNextItemOnShelf,
  kAmbientHighlightPreviousItemOnShelf,
  kAmbientMoveAppsInGrid,
  kAmbientMoveAppsInOutFolder,
  kAmbientRemoveHighlightOnShelf,
  kAmbientActivateIndexedDesk,
  kAmbientLaunchAppByIndex,
  kAmbientDisplayHiddenFiles,
  kAmbientOpenRightClickMenu,
  kAmbientCaretBrowsing,
  kAmbientSwitchFocus,  // DEPRECATED
  kAmbientCopy,
  kAmbientCut,
  kAmbientPaste,
  kAmbientPastePlainText,
  kAmbientDeletePreviousWord,
  kAmbientUndo,
  kAmbientRedo,
  kAmbientContentContextSelectAll,
  kAmbientSelectTextToBeginning,
  kAmbientSelectTextToEndOfLine,
  kAmbientSelectPreviousWord,
  kAMbientSelectNextWord,
  kAmbientDeleteNextWord,
  kAmbientGoToBeginningOfDocument,
  kAmbientGoToEndOfDocument,
  kAmbientGoToBeginningOfLine,
  kAmbientGoToEndOfLine,
  kAmbientMoveStartOfPreviousWord,
  kAmbientMoveToEndOfWord,
  kAmbientSwitchFocusForwards,
  kAmbientSwitchFocusBackwards,
};

// Contains details for UI styling of an accelerator.
struct AcceleratorLayoutDetails {
  // The accelerator action id associated for a source. Concat `source` and
  // `action_id` to get a unique identifier for an accelerator action.
  uint32_t action_id;

  // String id of the accelerator's description.
  int description_string_id;

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

// Contains info related to a non-configurable accelerator. A non-configurable
// accelerator can contain either a standard or text-based accelerator. The
// message_id and list of replacements will be provided when dealing
// with text-based accelerators; otherwise, accelerators will be provided
// and message_id/replacements should not have any value set.
// AcceleratorConfigurationProvider uses this struct to create a list of
// AcceleratorInfo struct's for each non-configurable action.
struct NonConfigurableAcceleratorDetails {
  NonConfigurableAcceleratorDetails(
      int message_id,
      std::vector<TextAcceleratorPart> replacements);
  explicit NonConfigurableAcceleratorDetails(int resource_id);
  explicit NonConfigurableAcceleratorDetails(
      std::vector<ui::Accelerator> accels);
  NonConfigurableAcceleratorDetails(const NonConfigurableAcceleratorDetails&);
  NonConfigurableAcceleratorDetails& operator=(
      const NonConfigurableAcceleratorDetails&);
  ~NonConfigurableAcceleratorDetails();

 public:
  bool IsStandardAccelerator() const { return accelerators.has_value(); }

  // These members are used for the Ambient action ids contained in
  // the NonConfigurableActions enum.
  std::optional<int> message_id;
  std::optional<std::vector<TextAcceleratorPart>> replacements;
  // This member is used for the Browser action ids contained in
  // the NonConfigurableActions enum.
  std::optional<std::vector<ui::Accelerator>> accelerators;
};

using NonConfigurableActionsMap =
    std::map<NonConfigurableActions, NonConfigurableAcceleratorDetails>;

using AcceleratorLayoutMap = std::map<uint32_t, AcceleratorLayoutDetails>;

using ReservedAcceleratorsMap = std::map<ui::Accelerator, uint32_t>;

const NonConfigurableActionsMap& GetNonConfigurableActionsMap();

const AcceleratorLayoutMap& GetAcceleratorLayoutMap();

const ReservedAcceleratorsMap& GetReservedAcceleratorsMap();

std::optional<AcceleratorLayoutDetails> GetAcceleratorLayout(uint32_t id);

// A fixed set of accelerators that should not have a layout. This is used for
// integrity check to make sure when a new accelerator is added, either it has
// been added to `kAcceleratorLayouts` or here.
constexpr auto kAshAcceleratorsWithoutLayout =
    base::MakeFixedFlatSet<AcceleratorAction>({
        AcceleratorAction::kCycleBackwardMru,
        AcceleratorAction::kCycleForwardMru,
        AcceleratorAction::kDisableCapsLock,
        AcceleratorAction::kFocusCameraPreview,
        AcceleratorAction::kFocusNextPane,
        AcceleratorAction::kLaunchApp0,
        AcceleratorAction::kLaunchApp1,
        AcceleratorAction::kLaunchApp2,
        AcceleratorAction::kLaunchApp3,
        AcceleratorAction::kLaunchApp4,
        AcceleratorAction::kLaunchApp5,
        AcceleratorAction::kLaunchApp6,
        AcceleratorAction::kLaunchApp7,
        AcceleratorAction::kLockPressed,
        AcceleratorAction::kLockReleased,
        AcceleratorAction::kMediaRewind,
        AcceleratorAction::kMediaStop,
        AcceleratorAction::kNewIncognitoWindow,
        AcceleratorAction::kNewTab,
        AcceleratorAction::kNewWindow,
        AcceleratorAction::kPasteClipboardHistoryPlainText,
        AcceleratorAction::kPowerPressed,
        AcceleratorAction::kPowerReleased,
        AcceleratorAction::kPrintUiHierarchies,
        AcceleratorAction::kRestoreTab,
        AcceleratorAction::kRotateWindow,
        AcceleratorAction::kToggleProjectorMarker,
        AcceleratorAction::kToggleWifi,
        AcceleratorAction::kTouchHudClear,
        AcceleratorAction::kTouchHudModeChange,
        AcceleratorAction::kVolumeMuteToggle,
        AcceleratorAction::kUnpin,
    });

// The following is an ordered list of accelerator layouts sorted by category
// and how they are represented in the app. If adding a new action, please
// ensure that the position is correct and the corresponding layout detail is
// represented in `acceleratorLayoutMap` in the .cc file.
inline constexpr uint32_t kAcceleratorLayouts[] = {
    // General
    // General > Controls
    AcceleratorAction::kToggleAppList,
    AcceleratorAction::kToggleOverview,
    AcceleratorAction::kToggleSystemTrayBubble,
    AcceleratorAction::kToggleCalendar,
    AcceleratorAction::kToggleMessageCenterBubble,
    AcceleratorAction::kTakeScreenshot,
    AcceleratorAction::kTakePartialScreenshot,
    AcceleratorAction::kTakeWindowScreenshot,
    AcceleratorAction::kStopScreenRecording,
    AcceleratorAction::kLockScreen,
    AcceleratorAction::kSuspend,
    AcceleratorAction::kExit,
    AcceleratorAction::kSwitchToNextUser,
    AcceleratorAction::kSwitchToPreviousUser,
    AcceleratorAction::kStartAssistant,

    // General > Apps
    AcceleratorAction::kOpenFileManager,
    NonConfigurableActions::kAmbientOpenFile,
    NonConfigurableActions::kAmbientDisplayHiddenFiles,
    AcceleratorAction::kShowShortcutViewer,
    AcceleratorAction::kOpenCalculator,
    AcceleratorAction::kOpenDiagnostics,
    AcceleratorAction::kOpenGetHelp,
    AcceleratorAction::kOpenFeedbackPage,
    NonConfigurableActions::kAmbientLaunchNumberedApp,
    AcceleratorAction::kLaunchLastApp,
    AcceleratorAction::kToggleResizeLockMenu,
    AcceleratorAction::kShowTaskManager,
    AcceleratorAction::kOpenCrosh,

    // Device
    // Device > Media
    AcceleratorAction::kVolumeUp,
    AcceleratorAction::kVolumeDown,
    AcceleratorAction::kVolumeMute,
    AcceleratorAction::kMicrophoneMuteToggle,
    AcceleratorAction::kMediaPlay,
    AcceleratorAction::kMediaPause,
    AcceleratorAction::kMediaPlayPause,
    AcceleratorAction::kMediaNextTrack,
    AcceleratorAction::kMediaPrevTrack,
    AcceleratorAction::kMediaFastForward,
    AcceleratorAction::kFocusPip,
    AcceleratorAction::kResizePipWindow,

    // Device > Input
    AcceleratorAction::kKeyboardBacklightToggle,
    AcceleratorAction::kKeyboardBrightnessUp,
    AcceleratorAction::kKeyboardBrightnessDown,
    AcceleratorAction::kToggleImeMenuBubble,
    AcceleratorAction::kSwitchToNextIme,
    AcceleratorAction::kSwitchToLastUsedIme,
    AcceleratorAction::kToggleStylusTools,

    // Device > Display
    AcceleratorAction::kBrightnessUp,
    AcceleratorAction::kBrightnessDown,
    AcceleratorAction::kScaleUiUp,
    AcceleratorAction::kScaleUiDown,
    AcceleratorAction::kScaleUiReset,
    AcceleratorAction::kPrivacyScreenToggle,
    AcceleratorAction::kToggleMirrorMode,
    AcceleratorAction::kSwapPrimaryDisplay,
    AcceleratorAction::kRotateScreen,

    // Browser
    // Browser > General
    NonConfigurableActions::kBrowserPrint,
    NonConfigurableActions::kBrowserShowAppMenu,
    NonConfigurableActions::kBrowserShowDownloads,
    NonConfigurableActions::kBrowserShowHistory,
    NonConfigurableActions::kBrowserClearBrowsingData,
    NonConfigurableActions::kBrowserOpenFile,

    // Browser > Browser Navigation
    NonConfigurableActions::kBrowserFocusAddressBar,
    NonConfigurableActions::kBrowserFocusSearch,
    NonConfigurableActions::kBrowserAutoComplete,
    NonConfigurableActions::kAmbientOpenPageInNewTab,
    NonConfigurableActions::kBrowserFocusMenuBar,
    NonConfigurableActions::kBrowserFocusBookmarks,
    NonConfigurableActions::kBrowserNextPane,
    AcceleratorAction::kFocusPreviousPane,

    // Browser > Pages
    NonConfigurableActions::kBrowserBack,
    NonConfigurableActions::kBrowserForward,
    NonConfigurableActions::kBrowserHome,
    NonConfigurableActions::kBrowserReload,
    NonConfigurableActions::kBrowserReloadBypassingCache,
    NonConfigurableActions::kBrowserPageUp,
    NonConfigurableActions::kBrowserPageDown,
    NonConfigurableActions::kBrowserTopPage,
    NonConfigurableActions::kBrowserBottomPage,
    NonConfigurableActions::kBrowserZoomPlus,
    NonConfigurableActions::kBrowserZoomMinus,
    NonConfigurableActions::kBrowserZoomNormal,
    NonConfigurableActions::kBrowserFind,
    NonConfigurableActions::kBrowserSavePage,
    NonConfigurableActions::kBrowserFindNext,
    NonConfigurableActions::kBrowserFindPrevious,

    // Browser > Tabs
    NonConfigurableActions::kBrowserNewTab,
    NonConfigurableActions::kBrowserSelectNextTab,
    NonConfigurableActions::kBrowserSelectPreviousTab,
    NonConfigurableActions::kBrowserNewWindow,
    NonConfigurableActions::kBrowserNewIncognitoWindow,
    NonConfigurableActions::kBrowserTabSearch,
    NonConfigurableActions::kBrowserCloseTab,
    NonConfigurableActions::kBrowserRestoreTab,
    NonConfigurableActions::kBrowserSelectLastTab,
    NonConfigurableActions::kBrowserSelectTabByIndex,
    NonConfigurableActions::kAmbientDragLinkInSameTab,
    NonConfigurableActions::kAmbientDragLinkInNewTab,
    NonConfigurableActions::kAmbientOpenLinkInWindow,
    NonConfigurableActions::kAmbientOpenLinkInTab,
    NonConfigurableActions::kAmbientOpenLinkInTabBackground,
    NonConfigurableActions::kBrowserStopDragTab,

    // Browser > Bookmarks
    NonConfigurableActions::kBrowserBookmarkThisTab,
    NonConfigurableActions::kAmbientSaveLinkAsBookmark,
    NonConfigurableActions::kBrowserBookmarkAllTabs,
    NonConfigurableActions::kBrowserShowBookmarkBar,
    NonConfigurableActions::kBrowserShowBookmarkManager,

    // Browser > Developer tools
    NonConfigurableActions::kBrowserDevToolsConsole,
    NonConfigurableActions::kBrowserDevToolsInspect,
    NonConfigurableActions::kBrowserDevTools,
    NonConfigurableActions::kBrowserViewSource,

    // Text
    // Text > Navigation
    NonConfigurableActions::kAmbientGoToBeginningOfDocument,
    NonConfigurableActions::kAmbientGoToEndOfDocument,
    NonConfigurableActions::kAmbientGoToBeginningOfLine,
    NonConfigurableActions::kAmbientGoToEndOfLine,
    NonConfigurableActions::kAmbientMoveStartOfPreviousWord,
    NonConfigurableActions::kAmbientMoveToEndOfWord,

    // Text > Text editing
    AcceleratorAction::kToggleCapsLock,
    AcceleratorAction::kShowEmojiPicker,
    AcceleratorAction::kTogglePicker,
    NonConfigurableActions::kAmbientCopy,
    NonConfigurableActions::kAmbientCut,
    NonConfigurableActions::kAmbientPaste,
    NonConfigurableActions::kAmbientPastePlainText,
    AcceleratorAction::kToggleClipboardHistory,
    NonConfigurableActions::kAmbientDeletePreviousWord,
    NonConfigurableActions::kAmbientUndo,
    NonConfigurableActions::kAmbientRedo,
    NonConfigurableActions::kAmbientContentContextSelectAll,
    NonConfigurableActions::kAmbientSelectTextToBeginning,
    NonConfigurableActions::kAmbientSelectTextToEndOfLine,
    NonConfigurableActions::kAmbientSelectPreviousWord,
    NonConfigurableActions::kAMbientSelectNextWord,
    NonConfigurableActions::kAmbientDeleteNextWord,

    // Windows and desks
    // Windows and desks > Windows
    NonConfigurableActions::kAmbientCycleForwardMRU,
    NonConfigurableActions::kAmbientCycleBackwardMRU,
    AcceleratorAction::kToggleMaximized,
    AcceleratorAction::kWindowMinimize,
    AcceleratorAction::kToggleFullscreen,
    NonConfigurableActions::kBrowserCloseWindow,
    AcceleratorAction::kToggleMultitaskMenu,
    AcceleratorAction::kWindowCycleSnapLeft,
    AcceleratorAction::kWindowCycleSnapRight,
    AcceleratorAction::kMoveActiveWindowBetweenDisplays,
    AcceleratorAction::kMinimizeTopWindowOnBack,
    AcceleratorAction::kCreateSnapGroup,
    AcceleratorAction::kToggleSnapGroupWindowsMinimizeAndRestore,
    AcceleratorAction::kToggleFloating,
    // TODO(b/343559364): Temporary location pending UI review.
    AcceleratorAction::kTilingWindowResizeLeft,
    AcceleratorAction::kTilingWindowResizeRight,
    AcceleratorAction::kTilingWindowResizeUp,
    AcceleratorAction::kTilingWindowResizeDown,

    // Windows and desks > Desks
    AcceleratorAction::kDesksNewDesk,
    AcceleratorAction::kDesksRemoveCurrentDesk,
    AcceleratorAction::kDesksActivateDeskLeft,
    AcceleratorAction::kDesksActivateDeskRight,
    AcceleratorAction::kDesksMoveActiveItemLeft,
    AcceleratorAction::kDesksMoveActiveItemRight,
    NonConfigurableActions::kAmbientActivateIndexedDesk,
    AcceleratorAction::kDesksToggleAssignToAllDesks,

    // Accessibility
    // Accessbility > ChromeVox
    // TODO(jimmyxgong): Allow this to be modifiable but only after revising the
    // notification that hardcodes ctrl + alt + z into the notification message.
    AcceleratorAction::kToggleSpokenFeedback,

    // Accessibility > Mouse Keys
    AcceleratorAction::kToggleMouseKeys,

    // Accessibility > Visibility
    AcceleratorAction::kEnableOrToggleDictation,
    AcceleratorAction::kEnableSelectToSpeak,
    AcceleratorAction::kToggleHighContrast,
    AcceleratorAction::kToggleDockedMagnifier,
    AcceleratorAction::kToggleFullscreenMagnifier,
    AcceleratorAction::kMagnifierZoomIn,
    AcceleratorAction::kMagnifierZoomOut,

    // Accessibility > Accessbility navigation
    AcceleratorAction::kAccessibilityAction,
    NonConfigurableActions::kAmbientSwitchFocusForwards,
    NonConfigurableActions::kAmbientSwitchFocusBackwards,
    NonConfigurableActions::kAmbientCaretBrowsing,
    AcceleratorAction::kFocusShelf,
    NonConfigurableActions::kAmbientHighlightNextItemOnShelf,
    NonConfigurableActions::kAmbientHighlightPreviousItemOnShelf,
    NonConfigurableActions::kAmbientOpenHighlightedItemOnShelf,
    NonConfigurableActions::kAmbientRemoveHighlightOnShelf,
    NonConfigurableActions::kAmbientOpenRightClickMenu,
    NonConfigurableActions::kBrowserFocusInactivePopupForAccessibility,
    NonConfigurableActions::kBrowserFocusToolbar,
    NonConfigurableActions::kAmbientMoveAppsInGrid,
    NonConfigurableActions::kAmbientMoveAppsInOutFolder,
};

}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_ACCELERATOR_LAYOUT_TABLE_H_
