// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "media/base/media_switches.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {

AcceleratorController* g_instance = nullptr;

base::RepeatingClosure* GetVolumeAdjustmentCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return callback.get();
}

}  // namespace

//  If you plan on adding a new accelerator and want it displayed in the
//  Shortcuts app, please follow the instructions at:
// `ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h`.
const AcceleratorData kAcceleratorData[] = {
    {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kSwitchToLastUsedIme},
    {false, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kSwitchToLastUsedIme},
    {true, ui::VKEY_TAB, ui::EF_ALT_DOWN, AcceleratorAction::kCycleForwardMru},
    {true, ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kCycleBackwardMru},
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE,
     AcceleratorAction::kToggleOverview},
    // Historically, the browser search key with and without the shift key can
    // toggle the app list into different open states. Now the two combinations
    // are used to toggle the app list in the same way to keep the behavior
    // consistent.
    {true, ui::VKEY_BROWSER_SEARCH, ui::EF_NONE,
     AcceleratorAction::kToggleAppList},
    {true, ui::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN,
     AcceleratorAction::kToggleAppList},
    {true, ui::VKEY_ALL_APPLICATIONS, ui::EF_NONE,
     AcceleratorAction::kToggleAppList},
    {true, ui::VKEY_WLAN, ui::EF_NONE, AcceleratorAction::kToggleWifi},
    {true, ui::VKEY_PRIVACY_SCREEN_TOGGLE, ui::EF_NONE,
     AcceleratorAction::kPrivacyScreenToggle},
    {true, ui::VKEY_MICROPHONE_MUTE_TOGGLE, ui::EF_NONE,
     AcceleratorAction::kMicrophoneMuteToggle},
    {true, ui::VKEY_M, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kMicrophoneMuteToggle},
    {true, ui::VKEY_KBD_BACKLIGHT_TOGGLE, ui::EF_NONE,
     AcceleratorAction::kKeyboardBacklightToggle},
    {true, ui::VKEY_KBD_BRIGHTNESS_DOWN, ui::EF_NONE,
     AcceleratorAction::kKeyboardBrightnessDown},
    {true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
     AcceleratorAction::kKeyboardBrightnessUp},
    // Maximize button.
    {true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kToggleMirrorMode},
    {true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
     AcceleratorAction::kSwapPrimaryDisplay},
    // Cycle windows button.
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kTakeScreenshot},
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kTakePartialScreenshot},
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kTakeWindowScreenshot},
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE,
     AcceleratorAction::kBrightnessDown},
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN,
     AcceleratorAction::kKeyboardBrightnessDown},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE,
     AcceleratorAction::kBrightnessUp},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
     AcceleratorAction::kKeyboardBrightnessUp},
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kMagnifierZoomOut},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kMagnifierZoomIn},
    {true, ui::VKEY_L, ui::EF_COMMAND_DOWN, AcceleratorAction::kLockScreen},
    {true, ui::VKEY_L, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kSuspend},
    // The lock key on Chrome OS keyboards produces F13 scancodes.
    {true, ui::VKEY_F13, ui::EF_NONE, AcceleratorAction::kLockPressed},
    {false, ui::VKEY_F13, ui::EF_NONE, AcceleratorAction::kLockReleased},
    // Generic keyboards can use VKEY_SLEEP to mimic ChromeOS keyboard's lock
    // key.
    {true, ui::VKEY_SLEEP, ui::EF_NONE, AcceleratorAction::kLockPressed},
    {false, ui::VKEY_SLEEP, ui::EF_NONE, AcceleratorAction::kLockReleased},
    {true, ui::VKEY_POWER, ui::EF_NONE, AcceleratorAction::kPowerPressed},
    {false, ui::VKEY_POWER, ui::EF_NONE, AcceleratorAction::kPowerReleased},
    {true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE,
     AcceleratorAction::kOpenCalculator},
    {true, ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kOpenDiagnostics},
    {true, ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kOpenFileManager},
    {true, ui::VKEY_H, ui::EF_COMMAND_DOWN, AcceleratorAction::kOpenGetHelp},
    {true, ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kOpenCrosh},
    {true, ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kTouchHudModeChange},
    {true, ui::VKEY_I,
     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kTouchHudClear},
    {true, ui::VKEY_H, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kToggleHighContrast},
    {true, ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kToggleSpokenFeedback},
    {true, ui::VKEY_S, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kEnableSelectToSpeak},
    {true, ui::VKEY_D, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kEnableOrToggleDictation},
    {true, ui::VKEY_DICTATE, ui::EF_NONE,
     AcceleratorAction::kEnableOrToggleDictation},
    {true, ui::VKEY_OEM_COMMA, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kSwitchToPreviousUser},
    {true, ui::VKEY_OEM_PERIOD, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kSwitchToNextUser},
    // Single shift release turns off caps lock.
    {false, ui::VKEY_LSHIFT, ui::EF_NONE, AcceleratorAction::kDisableCapsLock},
    {false, ui::VKEY_SHIFT, ui::EF_NONE, AcceleratorAction::kDisableCapsLock},
    {false, ui::VKEY_RSHIFT, ui::EF_NONE, AcceleratorAction::kDisableCapsLock},
    {true, ui::VKEY_C, ui::EF_COMMAND_DOWN, AcceleratorAction::kToggleCalendar},
    // Accelerators to toggle Caps Lock.
    {true, ui::VKEY_CAPITAL, ui::EF_NONE, AcceleratorAction::kToggleCapsLock},
    // The following is triggered when Search is released while Alt is still
    // down. The key_code here is LWIN (for search) and Alt is a modifier.
    {false, ui::VKEY_LWIN, ui::EF_ALT_DOWN, AcceleratorAction::kToggleCapsLock},
    {false, ui::VKEY_RWIN, ui::EF_ALT_DOWN, AcceleratorAction::kToggleCapsLock},
    // The following is triggered when Alt is released while search is still
    // down. The key_code here is MENU (for Alt) and Search is a modifier
    // (EF_COMMAND_DOWN is used for Search as a modifier).
    {false, ui::VKEY_MENU, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleCapsLock},
    {true, ui::VKEY_V, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleClipboardHistory},
    {true, ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kPasteClipboardHistoryPlainText},
    {true, ui::VKEY_VOLUME_MUTE, ui::EF_NONE, AcceleratorAction::kVolumeMute},
    {true, ui::VKEY_VOLUME_DOWN, ui::EF_NONE, AcceleratorAction::kVolumeDown},
    {true, ui::VKEY_VOLUME_UP, ui::EF_NONE, AcceleratorAction::kVolumeUp},
    {true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kShowTaskManager},
    {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kSwitchToNextIme},
    {true, ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kOpenFeedbackPage},
    {true, ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kOpenFeedbackPage},
    {true, ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kExit},
    {true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kNewIncognitoWindow},
    {true, ui::VKEY_N, ui::EF_CONTROL_DOWN, AcceleratorAction::kNewWindow},
    {true, ui::VKEY_T, ui::EF_CONTROL_DOWN, AcceleratorAction::kNewTab},
    {true, ui::VKEY_NEW, ui::EF_NONE, AcceleratorAction::kNewTab},
    {true, ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kScaleUiUp},
    {true, ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kScaleUiDown},
    {true, ui::VKEY_0, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kScaleUiReset},
    {true, ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kRotateScreen},
    {true, ui::VKEY_BROWSER_REFRESH,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kRotateWindow},
    {true, ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kRestoreTab},
    // This corresponds to the "Print Screen" key.
    {true, ui::VKEY_SNAPSHOT, ui::EF_NONE, AcceleratorAction::kTakeScreenshot},
    {true, ui::VKEY_SNAPSHOT, ui::EF_ALT_DOWN,
     AcceleratorAction::kTakePartialScreenshot},
    // On Chrome OS, Search key is mapped to LWIN. The Search key binding should
    // act on release instead of press when using Search as a modifier key for
    // extended keyboard shortcuts.
    {false, ui::VKEY_LWIN, ui::EF_NONE, AcceleratorAction::kToggleAppList},
    {false, ui::VKEY_LWIN, ui::EF_SHIFT_DOWN,
     AcceleratorAction::kToggleAppList},
    {false, ui::VKEY_RWIN, ui::EF_NONE, AcceleratorAction::kToggleAppList},
    {false, ui::VKEY_RWIN, ui::EF_SHIFT_DOWN,
     AcceleratorAction::kToggleAppList},
    {true, ui::VKEY_ZOOM, ui::EF_NONE, AcceleratorAction::kToggleFullscreen},
    {true, ui::VKEY_ZOOM, ui::EF_SHIFT_DOWN,
     AcceleratorAction::kToggleFullscreen},
    {true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kUnpin},
    {true, ui::VKEY_S, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kFocusCameraPreview},
    {true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kFocusShelf},
    {true, ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kFocusPip},
    {true, ui::VKEY_HELP, ui::EF_NONE, AcceleratorAction::kOpenGetHelp},
    {true, ui::VKEY_S, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kShowShortcutViewer},
    {true, ui::VKEY_F14, ui::EF_NONE, AcceleratorAction::kShowShortcutViewer},
    {true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kToggleMessageCenterBubble},
    {true, ui::VKEY_P, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kToggleStylusTools},
    {true, ui::VKEY_X, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kStopScreenRecording},
    {true, ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kToggleSystemTrayBubble},
    // Until we have unified settings and notifications the "hamburger"
    // key opens quick settings.
    {true, ui::VKEY_SETTINGS, ui::EF_NONE,
     AcceleratorAction::kToggleSystemTrayBubble},
    {true, ui::VKEY_K, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleImeMenuBubble},
    {true, ui::VKEY_1, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp0},
    {true, ui::VKEY_2, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp1},
    {true, ui::VKEY_3, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp2},
    {true, ui::VKEY_4, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp3},
    {true, ui::VKEY_5, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp4},
    {true, ui::VKEY_6, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp5},
    {true, ui::VKEY_7, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp6},
    {true, ui::VKEY_8, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchApp7},
    {true, ui::VKEY_9, ui::EF_ALT_DOWN, AcceleratorAction::kLaunchLastApp},

    // Window management shortcuts.
    {true, ui::VKEY_OEM_4, ui::EF_ALT_DOWN,
     AcceleratorAction::kWindowCycleSnapLeft},
    {true, ui::VKEY_OEM_6, ui::EF_ALT_DOWN,
     AcceleratorAction::kWindowCycleSnapRight},
    {true, ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN,
     AcceleratorAction::kWindowMinimize},
    {true, ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleFloating},
    {true, ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN,
     AcceleratorAction::kToggleMaximized},
    {true, ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kFocusNextPane},
    {true, ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kFocusPreviousPane},
    {true, ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kFocusNextPane},
    {true, ui::VKEY_BROWSER_BACK, ui::EF_NONE,
     AcceleratorAction::kMinimizeTopWindowOnBack},
    {true, ui::VKEY_G, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kCreateSnapGroup},
    {true, ui::VKEY_D, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleSnapGroupWindowsMinimizeAndRestore},
    {true, ui::VKEY_Z, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleMultitaskMenu},

    // Moving active window between displays shortcut.
    {true, ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kMoveActiveWindowBetweenDisplays},

    // Magnifiers shortcuts.
    {true, ui::VKEY_D, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kToggleDockedMagnifier},
    {true, ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kToggleFullscreenMagnifier},

    {true, ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kToggleMouseKeys},

    // Media Player shortcuts.
    {true, ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE,
     AcceleratorAction::kMediaNextTrack},
    {true, ui::VKEY_PAUSE, ui::EF_NONE, AcceleratorAction::kMediaPause},
    {true, ui::VKEY_PLAY, ui::EF_NONE, AcceleratorAction::kMediaPlay},
    {true, ui::VKEY_MEDIA_PAUSE, ui::EF_NONE, AcceleratorAction::kMediaPause},
    {true, ui::VKEY_MEDIA_PLAY, ui::EF_NONE, AcceleratorAction::kMediaPlay},
    {true, ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE,
     AcceleratorAction::kMediaPlayPause},
    {true, ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE,
     AcceleratorAction::kMediaPrevTrack},
    {true, ui::VKEY_MEDIA_STOP, ui::EF_NONE, AcceleratorAction::kMediaStop},
    {true, ui::VKEY_OEM_103, ui::EF_NONE, AcceleratorAction::kMediaRewind},
    {true, ui::VKEY_OEM_104, ui::EF_NONE, AcceleratorAction::kMediaFastForward},

    // Assistant shortcuts.
    {true, ui::VKEY_A, ui::EF_COMMAND_DOWN, AcceleratorAction::kStartAssistant},
    {true, ui::VKEY_ASSISTANT, ui::EF_NONE, AcceleratorAction::kStartAssistant},

    // IME mode change key.
    {true, ui::VKEY_MODECHANGE, ui::EF_NONE,
     AcceleratorAction::kSwitchToNextIme},

    // Emoji picker shortcut.
    {true, ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     AcceleratorAction::kShowEmojiPicker},
    {true, ui::VKEY_EMOJI_PICKER, ui::EF_NONE,
     AcceleratorAction::kShowEmojiPicker},

    // Debugging shortcuts that need to be available to end-users in
    // release builds.
    {true, ui::VKEY_U, kDebugModifier, AcceleratorAction::kPrintUiHierarchies},

    // Virtual Desks shortcuts.
    // Desk activation:
    {true, ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kDesksActivateDeskLeft},
    {true, ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kDesksActivateDeskRight},
    // Moving windows to desks:
    {true, ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kDesksMoveActiveItemLeft},
    {true, ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kDesksMoveActiveItemRight},
    // TODO(afakhry): Implement moving windows to a desk by its index directly.

    // TODO(yusukes): Handle VKEY_MEDIA_STOP, and VKEY_MEDIA_LAUNCH_MAIL.

    // PIP-resize shortcut.
    {true, ui::VKEY_X, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kResizePipWindow},

    // ARC-specific shortcut.
    {true, ui::VKEY_C, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kToggleResizeLockMenu},

    // Projector shortcuts.
    {true, ui::VKEY_OEM_3, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleProjectorMarker},

    // Accessibility key.
    {true, ui::VKEY_ACCESSIBILITY, ui::EF_NONE,
     AcceleratorAction::kAccessibilityAction},
};

const size_t kAcceleratorDataLength = std::size(kAcceleratorData);

const AcceleratorData kDisableWithNewMappingAcceleratorData[] = {
    // Desk creation and removal:
    // Due to https://crbug.com/976487, Search + "=" is always automatically
    // rewritten to F12, and so is Search + "-" to F11. So we had to implement
    // the following two shortcuts as Shift + F11/F12 until we resolve the above
    // issue, accepting the fact that these two shortcuts might sometimes be
    // consumed by apps and pages (since they're not search-based).
    // TODO(afakhry): Change the following to Search+Shift+"+"/"-" once
    // https://crbug.com/976487 is fixed.
    {true, ui::VKEY_F12, ui::EF_SHIFT_DOWN, AcceleratorAction::kDesksNewDesk},
    {true, ui::VKEY_F11, ui::EF_SHIFT_DOWN,
     AcceleratorAction::kDesksRemoveCurrentDesk},
};

const size_t kDisableWithNewMappingAcceleratorDataLength =
    std::size(kDisableWithNewMappingAcceleratorData);

const AcceleratorData kEnableWithPositionalAcceleratorsData[] = {
    // These are the desk shortcuts as advertised, but previously
    // they were implicitly implemented in terms of F11 and F12
    // due to event rewrites. Since the F-Key rewrites are deprecated
    // these can be implemented based on the keys they actually are.
    //
    // TODO(crbug.com/1179893): Merge these to the main table once
    // IsImprovedKeyboardShortcutsEnabled() is permanently enabled.
    {true, ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kDesksNewDesk},
    {true, ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kDesksRemoveCurrentDesk},
};

const size_t kEnableWithPositionalAcceleratorsDataLength =
    std::size(kEnableWithPositionalAcceleratorsData);

const AcceleratorData
    kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData[] = {
        // Indexed-desk activation:
        {true, ui::VKEY_1, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate0},
        {true, ui::VKEY_2, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate1},
        {true, ui::VKEY_3, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate2},
        {true, ui::VKEY_4, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate3},
        {true, ui::VKEY_5, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate4},
        {true, ui::VKEY_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate5},
        {true, ui::VKEY_7, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate6},
        {true, ui::VKEY_8, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksActivate7},
        // Toggle assign to all desks:
        {true, ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksToggleAssignToAllDesks},
};

const size_t kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength =
    std::size(kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData);

const AcceleratorData kEnableWithSameAppWindowCycleAcceleratorData[] = {
    {true, ui::VKEY_OEM_3, ui::EF_ALT_DOWN,
     AcceleratorAction::kCycleSameAppWindowsForward},
    {true, ui::VKEY_OEM_3, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kCycleSameAppWindowsBackward},
};

const size_t kEnableWithSameAppWindowCycleAcceleratorDataLength =
    std::size(kEnableWithSameAppWindowCycleAcceleratorData);

const AcceleratorData kToggleGameDashboardAcceleratorData[] = {
    {true, ui::VKEY_G, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleGameDashboard},
};

const size_t kToggleGameDashboardAcceleratorDataLength =
    std::size(kToggleGameDashboardAcceleratorData);

const AcceleratorData kTogglePickerAcceleratorData[] = {
    {false, ui::VKEY_RIGHT_ALT, ui::EF_NONE, AcceleratorAction::kTogglePicker,
     true},
    {true, ui::VKEY_F, ui::EF_COMMAND_DOWN, AcceleratorAction::kTogglePicker},
};

const size_t kTogglePickerAcceleratorDataLength =
    std::size(kTogglePickerAcceleratorData);

const AcceleratorData kTilingWindowResizeAcceleratorData[] = {
    {true, ui::VKEY_OEM_COMMA, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kTilingWindowResizeLeft},
    {true, ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kTilingWindowResizeRight},
    {true, ui::VKEY_OEM_1, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kTilingWindowResizeUp},
    {true, ui::VKEY_OEM_2, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kTilingWindowResizeDown},
};

const size_t kTilingWindowResizeAcceleratorDataLength =
    std::size(kTilingWindowResizeAcceleratorData);

// static
AcceleratorController* AcceleratorController::Get() {
  return g_instance;
}

// static
void AcceleratorController::SetVolumeAdjustmentSoundCallback(
    const base::RepeatingClosure& closure) {
  DCHECK(GetVolumeAdjustmentCallback()->is_null() || closure.is_null());
  *GetVolumeAdjustmentCallback() = std::move(closure);
}

// static
void AcceleratorController::PlayVolumeAdjustmentSound() {
  if (*GetVolumeAdjustmentCallback())
    GetVolumeAdjustmentCallback()->Run();
}

// static
bool AcceleratorController::IsSystemKey(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_ASSISTANT:
    case ui::VKEY_ZOOM:               // Fullscreen button.
    case ui::VKEY_MEDIA_LAUNCH_APP1:  // Overview button.
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_DOWN:
    case ui::VKEY_KBD_BRIGHTNESS_UP:
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_VOLUME_UP:
    case ui::VKEY_POWER:
    case ui::VKEY_SLEEP:
    case ui::VKEY_F13:  // Lock button on some chromebooks emits F13.
    case ui::VKEY_PRIVACY_SCREEN_TOGGLE:
    case ui::VKEY_SETTINGS:
      return true;
    case ui::VKEY_MEDIA_NEXT_TRACK:
    case ui::VKEY_MEDIA_PAUSE:
    case ui::VKEY_MEDIA_PLAY:
    case ui::VKEY_MEDIA_PLAY_PAUSE:
    case ui::VKEY_MEDIA_PREV_TRACK:
    case ui::VKEY_MEDIA_STOP:
    case ui::VKEY_OEM_103:  // KEYCODE_MEDIA_REWIND
    case ui::VKEY_OEM_104:  // KEYCODE_MEDIA_FAST_FORWARD
      return base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling);
    default:
      return false;
  }
}

void AcceleratorController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AcceleratorController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AcceleratorController::AcceleratorController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AcceleratorController::~AcceleratorController() {
  for (auto& obs : observers_)
    obs.OnAcceleratorControllerWillBeDestroyed(this);

  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void AcceleratorController::NotifyActionPerformed(AcceleratorAction action) {
  for (Observer& observer : observers_)
    observer.OnActionPerformed(action);
}

}  // namespace ash
