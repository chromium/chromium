// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATORS_H_
#define ASH_PUBLIC_CPP_ACCELERATORS_H_

#include <stddef.h>

#include <array>

#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
class Accelerator;
}

namespace ash {
class AcceleratorHistory;

// See documentation in ash/accelerators/accelerator_table.h.

struct AcceleratorData {
  bool trigger_on_press;
  ui::KeyboardCode keycode;
  int modifiers;
  AcceleratorAction action;
  bool accelerator_locked = false;
};

// A mask of all the modifiers used for debug accelerators.
ASH_PUBLIC_EXPORT constexpr int kDebugModifier =
    ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN;

// Accelerators handled by AcceleratorController.
// If you plan on adding a new accelerator and want it displayed in the
// Shortcuts app, please follow the instructions at:
// `ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h`.
ASH_PUBLIC_EXPORT inline constexpr auto kAcceleratorData = std::to_array<
    AcceleratorData>({
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

    {true, ui::VKEY_4, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
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

    // Assistant shortcut. Assistant has two shortcuts, a dedicated Assistant
    // key and Search+A. Search+A is defined below as
    // `kAssistantSearchPlusAAcceleratorData`.
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

    // Quick Insert.
    {false, ui::VKEY_QUICK_INSERT, ui::EF_NONE,
     AcceleratorAction::kTogglePicker, true},
    {true, ui::VKEY_F, ui::EF_COMMAND_DOWN, AcceleratorAction::kTogglePicker},

    // Game Dashboard shortcut.
    {true, ui::VKEY_G, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kToggleGameDashboard},

    // Sunfish-session.
    {true, ui::VKEY_SPACE, ui::EF_COMMAND_DOWN,
     AcceleratorAction::kStartSunfishSession},
});

ASH_PUBLIC_EXPORT inline constexpr AcceleratorData
    kAssistantSearchPlusAAcceleratorData[] = {
        {true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
         AcceleratorAction::kStartAssistant}};

// Accelerators that are enabled/disabled with new accelerator mapping.
// crbug.com/1067269
ASH_PUBLIC_EXPORT inline constexpr auto kDisableWithNewMappingAcceleratorData =
    std::to_array<AcceleratorData>({
        // Desk creation and removal:
        // Due to https://crbug.com/976487, Search + "=" is always automatically
        // rewritten to F12, and so is Search + "-" to F11. So we had to
        // implement
        // the following two shortcuts as Shift + F11/F12 until we resolve the
        // above
        // issue, accepting the fact that these two shortcuts might sometimes be
        // consumed by apps and pages (since they're not search-based).
        // TODO(afakhry): Change the following to Search+Shift+"+"/"-" once
        // https://crbug.com/976487 is fixed.
        {true, ui::VKEY_F12, ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksNewDesk},
        {true, ui::VKEY_F11, ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDesksRemoveCurrentDesk},
    });

// Accelerators that are enabled with positional shortcut mapping.
ASH_PUBLIC_EXPORT inline constexpr auto kEnableWithPositionalAcceleratorsData =
    std::to_array<AcceleratorData>({
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
    });

// Accelerators that are enabled with improved desks keyboards shortcuts.
ASH_PUBLIC_EXPORT inline constexpr auto
    kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData =
        std::to_array<AcceleratorData>({
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
        });

// Accelerators that are enabled with same app window cycling experiment.
ASH_PUBLIC_EXPORT inline constexpr auto
    kEnableWithSameAppWindowCycleAcceleratorData =
        std::to_array<AcceleratorData>({
            {true, ui::VKEY_OEM_3, ui::EF_ALT_DOWN,
             AcceleratorAction::kCycleSameAppWindowsForward},
            {true, ui::VKEY_OEM_3, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
             AcceleratorAction::kCycleSameAppWindowsBackward},
        });

ASH_PUBLIC_EXPORT inline constexpr auto kTilingWindowResizeAcceleratorData =
    std::to_array<AcceleratorData>({
        {true, ui::VKEY_OEM_COMMA, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
         AcceleratorAction::kTilingWindowResizeLeft},
        {true, ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
         AcceleratorAction::kTilingWindowResizeRight},
        {true, ui::VKEY_OEM_1, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
         AcceleratorAction::kTilingWindowResizeUp},
        {true, ui::VKEY_OEM_2, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
         AcceleratorAction::kTilingWindowResizeDown},
    });

ASH_PUBLIC_EXPORT inline constexpr AcceleratorData kGeminiAcceleratorData[] = {
    {true, ui::VKEY_F23, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kToggleGeminiApp, /*accelerator_locked=*/true},
};
ASH_PUBLIC_EXPORT inline constexpr size_t kGeminiAcceleratorDataLength =
    std::size(kGeminiAcceleratorData);

ASH_PUBLIC_EXPORT inline constexpr AcceleratorData
    kToggleDoNotDisturbAcceleratorData[] = {
        {true, ui::VKEY_DO_NOT_DISTURB, ui::EF_NONE,
         AcceleratorAction::kToggleDoNotDisturb},
};
ASH_PUBLIC_EXPORT inline constexpr size_t
    kToggleDoNotDisturbAcceleratorDataLength =
        std::size(kToggleDoNotDisturbAcceleratorData);

ASH_PUBLIC_EXPORT inline constexpr AcceleratorData
    kToggleCameraAllowedAcceleratorData[] = {
        {true, ui::VKEY_CAMERA_ACCESS_TOGGLE, ui::EF_NONE,
         AcceleratorAction::kToggleCameraAllowed},
};
ASH_PUBLIC_EXPORT inline constexpr size_t
    kToggleCameraAllowedAcceleratorDataLength =
        std::size(kToggleCameraAllowedAcceleratorData);

// The public-facing interface for accelerator handling, which is Ash's duty to
// implement.
class ASH_PUBLIC_EXPORT AcceleratorController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when `action` is performed.
    virtual void OnActionPerformed(AcceleratorAction action) = 0;
    // Invoked when `controller` is destroyed.
    virtual void OnAcceleratorControllerWillBeDestroyed(
        AcceleratorController* controller) {}
  };

  // Returns the singleton instance.
  static AcceleratorController* Get();

  // Called by Chrome to set the closure that should be run when the volume has
  // been adjusted (playing an audible tone when spoken feedback is enabled).
  static void SetVolumeAdjustmentSoundCallback(
      const base::RepeatingClosure& closure);

  // Called by Ash to run the closure from SetVolumeAdjustmentSoundCallback.
  static void PlayVolumeAdjustmentSound();

  // Returns true if |key_code| is a key usually handled directly by the shell.
  static bool IsSystemKey(ui::KeyboardCode key_code);

  // Activates the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  virtual bool Process(const ui::Accelerator& accelerator) = 0;

  // Returns true if the |accelerator| is deprecated. Deprecated accelerators
  // can be consumed by web contents if needed.
  virtual bool IsDeprecated(const ui::Accelerator& accelerator) const = 0;

  // Performs the specified action if it is enabled. Returns whether the action
  // was performed successfully.
  virtual bool PerformActionIfEnabled(AcceleratorAction action,
                                      const ui::Accelerator& accelerator) = 0;

  // Called by Chrome when a menu item accelerator has been triggered. Returns
  // true if the menu should close.
  virtual bool OnMenuAccelerator(const ui::Accelerator& accelerator) = 0;

  // Returns true if the |accelerator| is registered.
  virtual bool IsRegistered(const ui::Accelerator& accelerator) const = 0;

  // Returns the accelerator histotry.
  virtual AcceleratorHistory* GetAcceleratorHistory() = 0;

  // Returns true if the provided accelerator matches the provided accelerator
  // action.
  virtual bool DoesAcceleratorMatchAction(const ui::Accelerator& accelerator,
                                          const AcceleratorAction action) = 0;

  virtual void ApplyAcceleratorForTesting(
      const ui::Accelerator& accelerator) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  AcceleratorController();
  virtual ~AcceleratorController();
  void NotifyActionPerformed(AcceleratorAction action);

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

// The public facing interface for AcceleratorHistory, which is implemented in
// ash.
class ASH_PUBLIC_EXPORT AcceleratorHistory {
 public:
  // Stores |accelerator| if it's different than the currently stored one.
  virtual void StoreCurrentAccelerator(const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATORS_H_
