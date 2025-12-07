// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

#include <stddef.h>

#include <array>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/strings/grit/ash_strings.h"
#include "build/branding_buildflags.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

// The complete list of Ash accelerators is in ash/public/cpp/accelerators.h.
// This file mainly keeps track of special categories of accelerator.
//
// There are five classes of accelerators in Ash:
//
// Ash (OS) reserved:
// * Neither packaged apps nor web pages can cancel.
// * For example, power button.
// * See kReservedActions below.
//
// Ash (OS) preferred:
// * Fullscreen window can consume, but normal window can't.
// * For example, Alt-Tab window cycling.
// * See kPreferredActions below.
//
// Chrome OS system keys:
// * For legacy reasons, v1 apps can process and cancel. Otherwise handled
//   directly by Ash.
// * Brightness, volume control, etc.
// * See IsSystemKey() in ash/accelerators/accelerator_filter.cc.
//
// Browser reserved:
// * Packaged apps can cancel but web pages cannot.
// * For example, browser back and forward from first-row function keys.
// * See IsReservedCommandOrKey() in
//   chrome/browser/ui/browser_command_controller.cc.
//
// Browser non-reserved:
// * Both packaged apps and web pages can cancel.
// * For example, selecting tabs by number with Ctrl-1 to Ctrl-9.
// * See kAcceleratorMap in chrome/browser/ui/views/accelerator_table.cc.
//
// In particular, there is not an accelerator processing pass for Ash after
// the browser gets the accelerator.  See crbug.com/285308 for details.
//
// There are also various restrictions on accelerators allowed at the login
// screen, when running in "forced app mode" (like a kiosk), etc. See the
// various kActionsAllowed* below.

// Gathers the needed data to handle deprecated accelerators.
struct DeprecatedAcceleratorData {
  // The action that has deprecated accelerators.
  AcceleratorAction action;

  // The name of the UMA histogram that will be used to measure the deprecated
  // v.s. new accelerator usage.
  const char* uma_histogram_name;

  // The ID of the localized notification message to show to users informing
  // them about the deprecation.
  int notification_message_id;

  // The ID of the localized new shortcut key.
  int new_shortcut_id;

  // The replacement of the deprecated accelerator.
  ui::Accelerator replacement;

  // Specifies whether the deprecated accelerator is still enabled to do its
  // associated action.
  bool deprecated_enabled;

  // The accelerator name in the pref dict to check if a deprecated accelerator
  // notification is displayed 3 times or within the last 24 hours.
  const char* pref_name;
};

// This will be used for the UMA stats to measure the how many users are using
// the old v.s. new accelerators.
enum DeprecatedAcceleratorUsage {
  DEPRECATED_USED = 0,     // The deprecated accelerator is used.
  NEW_USED,                // The new accelerator is used.
  DEPRECATED_USAGE_COUNT,  // Maximum value of this enum for histogram use.
};

// The list of the deprecated accelerators.
// Instructions for how to deprecate and replace an Accelerator:
//
// 1- Replace the old deprecated accelerator from the above list with the new
//    accelerator that will take its place.
// 2- Add an entry for it in the following |kDeprecatedAccelerators| list.
// 3- Add another entry in the |kDeprecatedAcceleratorsData|.
// 4- That entry should contain the following:
//    - The action that the deprecated accelerator maps to.
//    - Define a histogram for this action in |histograms.xml| in the form
//      "Ash.Accelerators.Deprecated.{ActionName}" and include the name of this
//      histogram in this entry. This name will be used as the ID of the
//      notification to be shown to the user. This is to prevent duplication of
//      same notification.
//    - The ID of the localized notification message to give the users telling
//      them about the deprecation (Add one in |ash_strings.grd|. Search for
//      the comment <!-- Deprecated Accelerators Messages -->).
//    - The IDs of the localized old and new shortcut text to be used to fill
//      the notification text. Also found in |ash_strings.grd|.
//    - {true or false} whether the deprecated accelerator is still enabled (we
//      don't disable a deprecated accelerator abruptly).
ASH_EXPORT inline constexpr auto kDeprecatedAccelerators =
    std::to_array<AcceleratorData>({
        {true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
         AcceleratorAction::kShowShortcutViewer},
        {true, ui::VKEY_OEM_2,
         ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kShowShortcutViewer},
        {true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN,
         AcceleratorAction::kOpenGetHelp},
        {true, ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
         AcceleratorAction::kOpenGetHelp},
    });

// The list of the actions with deprecated accelerators and the needed data to
// handle them.
// When removing entries from kDeprecatedAcceleratorsData, also clean up their
// prefs in kDeprecatedAcceleratorNotificationsShownCounts and
// kDeprecatedAcceleratorNotificationsLastShown.
ASH_EXPORT inline constexpr auto kDeprecatedAcceleratorsData =
    std::to_array<DeprecatedAcceleratorData>(
        {{AcceleratorAction::kShowShortcutViewer,
          "Ash.Accelerators.Deprecated.ShowShortcutViewer",
          IDS_DEPRECATED_SHOW_SHORTCUT_VIEWER_MSG,
          IDS_SHORTCUT_SHOW_SHORTCUT_VIEWER_NEW,
          ui::Accelerator(ui::VKEY_S,
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
          false, "show_shortcut_viewer"},
         {AcceleratorAction::kOpenGetHelp,
          "Ash.Accelerators.Deprecated.ShowShortcutViewer",
          IDS_DEPRECATED_OPEN_GET_HELP_MSG, IDS_SHORTCUT_OPEN_GET_HELP_NEW,
          ui::Accelerator(ui::VKEY_H, ui::EF_COMMAND_DOWN), false,
          "open_get_help"}});

// Debug accelerators. Debug accelerators are only enabled when the "Debugging
// keyboard shortcuts" flag (--ash-debug-shortcuts) is enabled. Debug actions
// are always run (similar to reserved actions). Debug accelerators can be
// enabled in about:flags.
ASH_EXPORT inline constexpr auto kDebugAcceleratorData =
    std::to_array<AcceleratorData>({
        {true, ui::VKEY_N, kDebugModifier, AcceleratorAction::kToggleWifi},
        {true, ui::VKEY_X, kDebugModifier,
         AcceleratorAction::kDebugKeyboardBacklightToggle},
        {true, ui::VKEY_M, kDebugModifier,
         AcceleratorAction::kDebugMicrophoneMuteToggle},
        {true, ui::VKEY_9, kDebugModifier,
         AcceleratorAction::kDebugShowInformedRestore},
        {true, ui::VKEY_O, kDebugModifier, AcceleratorAction::kDebugShowToast},
        {true, ui::VKEY_J, kDebugModifier,
         AcceleratorAction::kDebugShowSystemNudge},
        {true, ui::VKEY_Z, kDebugModifier,
         AcceleratorAction::kDebugSystemUiStyleViewer},
        {true, ui::VKEY_P, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDebugToggleTouchPad},
        {true, ui::VKEY_T, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         AcceleratorAction::kDebugToggleTouchScreen},
        {true, ui::VKEY_T, kDebugModifier,
         AcceleratorAction::kDebugToggleTabletMode},
        {true, ui::VKEY_A, kDebugModifier,
         AcceleratorAction::kDebugToggleVideoConferenceCameraTrayIcon},
        {true, ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
         AcceleratorAction::kDebugToggleWallpaperMode},
        {true, ui::VKEY_L, kDebugModifier,
         AcceleratorAction::kDebugPrintLayerHierarchy},
        {true, ui::VKEY_V, kDebugModifier,
         AcceleratorAction::kDebugPrintViewHierarchy},
        {true, ui::VKEY_W, kDebugModifier,
         AcceleratorAction::kDebugPrintWindowHierarchy},
        {true, ui::VKEY_B, kDebugModifier,
         AcceleratorAction::kDebugToggleShowDebugBorders},
        {true, ui::VKEY_F, kDebugModifier,
         AcceleratorAction::kDebugToggleShowFpsCounter},
        {true, ui::VKEY_P, kDebugModifier,
         AcceleratorAction::kDebugToggleShowPaintRects},
        {true, ui::VKEY_K, kDebugModifier,
         AcceleratorAction::kDebugTriggerCrash},
        {true, ui::VKEY_G, kDebugModifier,
         AcceleratorAction::kDebugToggleHudDisplay},
        {true, ui::VKEY_Q, kDebugModifier,
         AcceleratorAction::kDebugToggleVirtualTrackpad},
        {true, ui::VKEY_D, kDebugModifier,
         AcceleratorAction::kDebugToggleDarkMode},
        {true, ui::VKEY_Y, kDebugModifier,
         AcceleratorAction::kDebugToggleDynamicColor},
        {true, ui::VKEY_E, kDebugModifier,
         AcceleratorAction::kDebugTogglePowerButtonMenu},
        {true, ui::VKEY_C, kDebugModifier,
         AcceleratorAction::kDebugClearUseKMeansPref},
        {true, ui::VKEY_H, kDebugModifier,
         AcceleratorAction::kDebugToggleFocusModeState},
        {true, ui::VKEY_8, kDebugModifier,
         AcceleratorAction::kDebugStartSunfishSession},
        {true, ui::VKEY_0, kDebugModifier,
         AcceleratorAction::kDebugShowTestWindow},
    });

// Developer accelerators that are enabled only with the command-line switch
// --ash-dev-shortcuts. They are always run similar to reserved actions.
ASH_EXPORT inline constexpr auto kDeveloperAcceleratorData = std::to_array<
    AcceleratorData>({
    // Extra shortcut for debug build to control magnifier on Linux desktop.
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kMagnifierZoomOut},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN,
     AcceleratorAction::kMagnifierZoomIn},
    // Extra shortcuts to lock the screen on Linux desktop.
    {true, ui::VKEY_L, ui::EF_ALT_DOWN, AcceleratorAction::kLockPressed},
    {false, ui::VKEY_L, ui::EF_ALT_DOWN, AcceleratorAction::kLockReleased},
    {true, ui::VKEY_P, ui::EF_ALT_DOWN, AcceleratorAction::kPowerPressed},
    {false, ui::VKEY_P, ui::EF_ALT_DOWN, AcceleratorAction::kPowerReleased},
    {true, ui::VKEY_POWER, ui::EF_SHIFT_DOWN, AcceleratorAction::kLockPressed},
    {false, ui::VKEY_POWER, ui::EF_SHIFT_DOWN,
     AcceleratorAction::kLockReleased},
    {true, ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kDevAddRemoveDisplay},
    {true, ui::VKEY_U, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kDevToggleUnifiedDesktop},
    {true, ui::VKEY_M, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     AcceleratorAction::kToggleMirrorMode},
    {true, ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kToggleWifi},
    // Extra shortcut for display swapping as Alt-F4 is taken on Linux desktop.
    {true, ui::VKEY_S, kDebugModifier, AcceleratorAction::kSwapPrimaryDisplay},
    // Extra shortcut to rotate/scale up/down the screen on Linux desktop.
    {true, ui::VKEY_R, kDebugModifier, AcceleratorAction::kRotateScreen},
    // For testing on systems where Alt-Tab is already mapped.
    {true, ui::VKEY_W, ui::EF_ALT_DOWN, AcceleratorAction::kCycleForwardMru},
    {true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     AcceleratorAction::kCycleBackwardMru},
    {true, ui::VKEY_F, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     AcceleratorAction::kToggleFullscreen},
    // For testing on Linux desktop where it's hard to rebind the caps lock key.
    {true, ui::VKEY_A, ui::EF_ALT_DOWN, AcceleratorAction::kDevToggleAppList},
    {true, ui::VKEY_S, ui::EF_ALT_DOWN, AcceleratorAction::kToggleQuickInsert},

    // For testing fingerprint ui.
    {true, ui::VKEY_1, kDebugModifier, kTouchFingerprintSensor1},
    {true, ui::VKEY_2, kDebugModifier, kTouchFingerprintSensor2},
    {true, ui::VKEY_3, kDebugModifier, kTouchFingerprintSensor3},
});

// Actions that should be handled very early in Ash unless the current target
// window is full-screen.
ASH_EXPORT inline constexpr std::array kPreferredActions = {
    // Window cycling accelerators.
    AcceleratorAction::kCycleBackwardMru,             // Shift+Alt+Tab
    AcceleratorAction::kCycleForwardMru,              // Alt+Tab
    AcceleratorAction::kCycleSameAppWindowsBackward,  // Shift+Alt+Backtick
    AcceleratorAction::kCycleSameAppWindowsForward,   // Alt+Backtick
    // Pass through debug shortcuts so that these works on Fullscreen Chrome
    // Remote Desktop.
    AcceleratorAction::kDebugPrintLayerHierarchy,
    AcceleratorAction::kDebugPrintViewHierarchy,
    AcceleratorAction::kDebugPrintWindowHierarchy,
    AcceleratorAction::kDebugShowSystemNudge,
    AcceleratorAction::kDebugToggleHudDisplay,
    AcceleratorAction::kDebugToggleTouchPad,
    AcceleratorAction::kDebugToggleTouchScreen,
    AcceleratorAction::kDebugToggleTabletMode,
};

// Actions that are always handled in Ash.
ASH_EXPORT inline constexpr std::array kReservedActions = {
    AcceleratorAction::kPowerPressed, AcceleratorAction::kPowerReleased,
    AcceleratorAction::kLockPressed,  AcceleratorAction::kLockReleased,
    AcceleratorAction::kSuspend,      AcceleratorAction::kLockScreen,
};

// Actions allowed while user is not signed in or screen is locked.
ASH_EXPORT inline constexpr std::array kActionsAllowedAtLoginOrLockScreen = {
    AcceleratorAction::kBrightnessDown,
    AcceleratorAction::kBrightnessUp,
    AcceleratorAction::kDebugPrintLayerHierarchy,
    AcceleratorAction::kDebugPrintViewHierarchy,
    AcceleratorAction::kDebugPrintWindowHierarchy,
    AcceleratorAction::kDebugShowSystemNudge,
    AcceleratorAction::kDebugToggleHudDisplay,
    AcceleratorAction::kDebugToggleTouchPad,
    AcceleratorAction::kDebugToggleTouchScreen,
    AcceleratorAction::kDebugToggleTabletMode,
    AcceleratorAction::kDevAddRemoveDisplay,
    AcceleratorAction::kDisableCapsLock,
    AcceleratorAction::kEnableSelectToSpeak,
    AcceleratorAction::kEnableOrToggleDictation,
    AcceleratorAction::kKeyboardBacklightToggle,
    AcceleratorAction::kKeyboardBrightnessDown,
    AcceleratorAction::kKeyboardBrightnessUp,
    AcceleratorAction::kMagnifierZoomIn,   // Control+F7
    AcceleratorAction::kMagnifierZoomOut,  // Control+F6
    AcceleratorAction::kMediaFastForward,
    AcceleratorAction::kMediaNextTrack,
    AcceleratorAction::kMediaPause,
    AcceleratorAction::kMediaPlay,
    AcceleratorAction::kMediaPlayPause,
    AcceleratorAction::kMediaPrevTrack,
    AcceleratorAction::kMediaRewind,
    AcceleratorAction::kMediaStop,
    AcceleratorAction::kMicrophoneMuteToggle,
    AcceleratorAction::kPrivacyScreenToggle,
    AcceleratorAction::kPrintUiHierarchies,
    AcceleratorAction::kRotateScreen,
    AcceleratorAction::kScaleUiDown,
    AcceleratorAction::kScaleUiReset,
    AcceleratorAction::kScaleUiUp,
    AcceleratorAction::kToggleImeMenuBubble,
    AcceleratorAction::kSwitchToLastUsedIme,
    AcceleratorAction::kSwitchToNextIme,
    AcceleratorAction::kTakeScreenshot,
    AcceleratorAction::kToggleCalendar,
    AcceleratorAction::kToggleCapsLock,
    AcceleratorAction::kToggleDockedMagnifier,
    AcceleratorAction::kToggleFullscreenMagnifier,
    AcceleratorAction::kToggleHighContrast,
    AcceleratorAction::kToggleMirrorMode,
    AcceleratorAction::kToggleQuickInsert,
    AcceleratorAction::kToggleSpokenFeedback,
    AcceleratorAction::kToggleSystemTrayBubble,
    AcceleratorAction::kToggleWifi,
    AcceleratorAction::kTouchHudClear,
    AcceleratorAction::kTouchFingerprintSensor1,
    AcceleratorAction::kTouchFingerprintSensor2,
    AcceleratorAction::kTouchFingerprintSensor3,
    AcceleratorAction::kVolumeDown,
    AcceleratorAction::kVolumeMute,
    AcceleratorAction::kVolumeUp,
#if !defined(NDEBUG)
    AcceleratorAction::kPowerPressed,
    AcceleratorAction::kPowerReleased,
#endif  // !defined(NDEBUG)
};

// Actions allowed while screen is locked (in addition to
// kActionsAllowedAtLoginOrLockScreen).
ASH_EXPORT inline constexpr std::array kActionsAllowedAtLockScreen = {
    AcceleratorAction::kDebugToggleFocusModeState,
    AcceleratorAction::kExit,
    AcceleratorAction::kSuspend,
};

// Actions allowed while power menu is opened.
ASH_EXPORT inline constexpr std::array kActionsAllowedAtPowerMenu = {
    AcceleratorAction::kBrightnessDown, AcceleratorAction::kBrightnessUp,
    AcceleratorAction::kVolumeDown,     AcceleratorAction::kVolumeUp,
    AcceleratorAction::kVolumeMute,
};

// Actions allowed while a modal window is up.
ASH_EXPORT inline constexpr std::array kActionsAllowedAtModalWindow = {
    AcceleratorAction::kBrightnessDown,
    AcceleratorAction::kBrightnessUp,
    AcceleratorAction::kDebugKeyboardBacklightToggle,
    AcceleratorAction::kDebugMicrophoneMuteToggle,
    AcceleratorAction::kDebugToggleTouchPad,
    AcceleratorAction::kDebugToggleTouchScreen,
    AcceleratorAction::kDevAddRemoveDisplay,
    AcceleratorAction::kDisableCapsLock,
    AcceleratorAction::kEnableSelectToSpeak,
    AcceleratorAction::kEnableOrToggleDictation,
    AcceleratorAction::kExit,
    AcceleratorAction::kKeyboardBacklightToggle,
    AcceleratorAction::kKeyboardBrightnessDown,
    AcceleratorAction::kKeyboardBrightnessUp,
    AcceleratorAction::kLockScreen,
    AcceleratorAction::kMagnifierZoomIn,
    AcceleratorAction::kMagnifierZoomOut,
    AcceleratorAction::kMediaFastForward,
    AcceleratorAction::kMediaNextTrack,
    AcceleratorAction::kMediaPause,
    AcceleratorAction::kMediaPlay,
    AcceleratorAction::kMediaPlayPause,
    AcceleratorAction::kMediaPrevTrack,
    AcceleratorAction::kMediaRewind,
    AcceleratorAction::kMediaStop,
    AcceleratorAction::kMicrophoneMuteToggle,
    AcceleratorAction::kOpenFeedbackPage,
    AcceleratorAction::kPowerPressed,
    AcceleratorAction::kPowerReleased,
    AcceleratorAction::kPrintUiHierarchies,
    AcceleratorAction::kPrivacyScreenToggle,
    AcceleratorAction::kRotateScreen,
    AcceleratorAction::kScaleUiDown,
    AcceleratorAction::kScaleUiReset,
    AcceleratorAction::kScaleUiUp,
    AcceleratorAction::kToggleImeMenuBubble,
    AcceleratorAction::kShowShortcutViewer,
    AcceleratorAction::kSuspend,
    AcceleratorAction::kSwapPrimaryDisplay,
    AcceleratorAction::kSwitchToLastUsedIme,
    AcceleratorAction::kSwitchToNextIme,
    AcceleratorAction::kTakePartialScreenshot,
    AcceleratorAction::kTakeScreenshot,
    AcceleratorAction::kTakeWindowScreenshot,
    AcceleratorAction::kToggleCapsLock,
    AcceleratorAction::kToggleDockedMagnifier,
    AcceleratorAction::kToggleFullscreenMagnifier,
    AcceleratorAction::kToggleHighContrast,
    AcceleratorAction::kToggleMirrorMode,
    AcceleratorAction::kToggleSpokenFeedback,
    AcceleratorAction::kToggleQuickInsert,
    AcceleratorAction::kToggleWifi,
    AcceleratorAction::kTouchFingerprintSensor1,
    AcceleratorAction::kTouchFingerprintSensor2,
    AcceleratorAction::kTouchFingerprintSensor3,
    AcceleratorAction::kVolumeDown,
    AcceleratorAction::kVolumeMute,
    AcceleratorAction::kVolumeUp,
};

// Actions which may be repeated by holding an accelerator key.
ASH_EXPORT inline constexpr std::array kRepeatableActions = {
    AcceleratorAction::kBrightnessDown,
    AcceleratorAction::kBrightnessUp,
    AcceleratorAction::kFocusNextPane,
    AcceleratorAction::kFocusPreviousPane,
    AcceleratorAction::kKeyboardBrightnessDown,
    AcceleratorAction::kKeyboardBrightnessUp,
    AcceleratorAction::kMagnifierZoomIn,
    AcceleratorAction::kMagnifierZoomOut,
    AcceleratorAction::kMediaFastForward,
    AcceleratorAction::kMediaNextTrack,
    AcceleratorAction::kMediaPrevTrack,
    AcceleratorAction::kMediaRewind,
    AcceleratorAction::kRestoreTab,
    AcceleratorAction::kTilingWindowResizeDown,
    AcceleratorAction::kTilingWindowResizeLeft,
    AcceleratorAction::kTilingWindowResizeRight,
    AcceleratorAction::kTilingWindowResizeUp,
    AcceleratorAction::kVolumeDown,
    AcceleratorAction::kVolumeUp,
};

// Actions allowed in app mode or pinned mode.
ASH_EXPORT inline constexpr std::array kActionsAllowedInAppModeOrPinnedMode = {
    AcceleratorAction::kBrightnessDown,
    AcceleratorAction::kBrightnessUp,
    AcceleratorAction::kDebugKeyboardBacklightToggle,
    AcceleratorAction::kDebugMicrophoneMuteToggle,
    AcceleratorAction::kDebugPrintLayerHierarchy,
    AcceleratorAction::kDebugPrintViewHierarchy,
    AcceleratorAction::kDebugPrintWindowHierarchy,
    AcceleratorAction::kDebugToggleTouchPad,
    AcceleratorAction::kDebugToggleTouchScreen,
    AcceleratorAction::kDevAddRemoveDisplay,
    AcceleratorAction::kDisableCapsLock,
    AcceleratorAction::kEnableSelectToSpeak,
    AcceleratorAction::kEnableOrToggleDictation,
    AcceleratorAction::kKeyboardBacklightToggle,
    AcceleratorAction::kKeyboardBrightnessDown,
    AcceleratorAction::kKeyboardBrightnessUp,
    AcceleratorAction::kMagnifierZoomIn,   // Control+F7
    AcceleratorAction::kMagnifierZoomOut,  // Control+F6
    AcceleratorAction::kMediaFastForward,
    AcceleratorAction::kMediaNextTrack,
    AcceleratorAction::kMediaPause,
    AcceleratorAction::kMediaPlay,
    AcceleratorAction::kMediaPlayPause,
    AcceleratorAction::kMediaPrevTrack,
    AcceleratorAction::kMediaRewind,
    AcceleratorAction::kMediaStop,
    AcceleratorAction::kMicrophoneMuteToggle,
    AcceleratorAction::kPasteClipboardHistoryPlainText,
    AcceleratorAction::kPowerPressed,
    AcceleratorAction::kPowerReleased,
    AcceleratorAction::kPrintUiHierarchies,
    AcceleratorAction::kPrivacyScreenToggle,
    AcceleratorAction::kRotateScreen,
    AcceleratorAction::kScaleUiDown,
    AcceleratorAction::kScaleUiReset,
    AcceleratorAction::kScaleUiUp,
    AcceleratorAction::kSwapPrimaryDisplay,
    AcceleratorAction::kSwitchToLastUsedIme,
    AcceleratorAction::kSwitchToNextIme,
    AcceleratorAction::kToggleCapsLock,
    AcceleratorAction::kToggleClipboardHistory,
    AcceleratorAction::kToggleDockedMagnifier,
    AcceleratorAction::kToggleFullscreenMagnifier,
    AcceleratorAction::kToggleHighContrast,
    AcceleratorAction::kToggleMirrorMode,
    AcceleratorAction::kToggleSpokenFeedback,
    AcceleratorAction::kToggleWifi,
    AcceleratorAction::kTouchHudClear,
    AcceleratorAction::kVolumeDown,
    AcceleratorAction::kVolumeMute,
    AcceleratorAction::kVolumeUp,
};

// Actions that can be performed in pinned mode.
// In pinned mode, the action listed in this or "in app mode or pinned mode"
// table can be performed.
ASH_EXPORT inline constexpr std::array kActionsAllowedInPinnedMode = {
    AcceleratorAction::kLockScreen,
    AcceleratorAction::kSuspend,
    AcceleratorAction::kTakePartialScreenshot,
    AcceleratorAction::kTakeScreenshot,
    AcceleratorAction::kTakeWindowScreenshot,
    AcceleratorAction::kUnpin,
};

// Actions that can be performed in app mode.
// In app mode, the action listed in this or "in app mode or pinned mode" table
// can be performed.
ASH_EXPORT inline constexpr std::array kActionsAllowedInAppMode = {
    AcceleratorAction::kFocusShelf,
};

// Actions that require at least 1 window.
ASH_EXPORT inline constexpr std::array kActionsNeedingWindow = {
    // clang-format off
    AcceleratorAction::kDesksMoveActiveItemLeft,
    AcceleratorAction::kDesksMoveActiveItemRight,
    AcceleratorAction::kDesksToggleAssignToAllDesks,
    AcceleratorAction::kMoveActiveWindowBetweenDisplays,
    AcceleratorAction::kRotateWindow,
    AcceleratorAction::kTilingWindowResizeDown,
    AcceleratorAction::kTilingWindowResizeLeft,
    AcceleratorAction::kTilingWindowResizeRight,
    AcceleratorAction::kTilingWindowResizeUp,
    AcceleratorAction::kToggleFloating,
    AcceleratorAction::kToggleFullscreen,
    AcceleratorAction::kToggleMaximized,
    AcceleratorAction::kToggleSnapGroup,
    AcceleratorAction::kToggleSnapGroupWindowsMinimizeAndRestore,
    AcceleratorAction::kWindowCycleSnapLeft,
    AcceleratorAction::kWindowCycleSnapRight,
    AcceleratorAction::kWindowMinimize,
    // clang-format on
};

// Actions that can be performed while keeping the menu open.
ASH_EXPORT inline constexpr std::array kActionsKeepingMenuOpen = {
    AcceleratorAction::kBrightnessDown,
    AcceleratorAction::kBrightnessUp,
    AcceleratorAction::kDebugKeyboardBacklightToggle,
    AcceleratorAction::kDebugMicrophoneMuteToggle,
    AcceleratorAction::kDebugToggleTouchPad,
    AcceleratorAction::kDebugToggleTouchScreen,
    // Keep the menu open when switching desks. The desk activation code will
    // close the menu without animation manually. Otherwise, the menu will fade
    // out and a trace will be visible while switching desks.
    AcceleratorAction::kDesksActivateDeskLeft,
    AcceleratorAction::kDesksActivateDeskRight,
    AcceleratorAction::kDesksNewDesk,
    AcceleratorAction::kDesksRemoveCurrentDesk,
    AcceleratorAction::kDisableCapsLock,
    AcceleratorAction::kEnableSelectToSpeak,
    AcceleratorAction::kEnableOrToggleDictation,
    AcceleratorAction::kKeyboardBacklightToggle,
    AcceleratorAction::kKeyboardBrightnessDown,
    AcceleratorAction::kKeyboardBrightnessUp,
    AcceleratorAction::kMediaFastForward,
    AcceleratorAction::kMediaNextTrack,
    AcceleratorAction::kMediaPause,
    AcceleratorAction::kMediaPlay,
    AcceleratorAction::kMediaPlayPause,
    AcceleratorAction::kMediaPrevTrack,
    AcceleratorAction::kMediaRewind,
    AcceleratorAction::kMediaStop,
    AcceleratorAction::kMicrophoneMuteToggle,
    AcceleratorAction::kPasteClipboardHistoryPlainText,
    AcceleratorAction::kPrintUiHierarchies,
    AcceleratorAction::kPrivacyScreenToggle,
    AcceleratorAction::kSwitchToLastUsedIme,
    AcceleratorAction::kSwitchToNextIme,
    AcceleratorAction::kTakePartialScreenshot,
    AcceleratorAction::kTakeScreenshot,
    AcceleratorAction::kTakeWindowScreenshot,
    AcceleratorAction::kToggleAppList,
    AcceleratorAction::kToggleCapsLock,
    AcceleratorAction::kToggleClipboardHistory,
    AcceleratorAction::kToggleDockedMagnifier,
    AcceleratorAction::kToggleFullscreenMagnifier,
    AcceleratorAction::kToggleHighContrast,
    AcceleratorAction::kToggleSpokenFeedback,
    AcceleratorAction::kToggleWifi,
    AcceleratorAction::kVolumeDown,
    AcceleratorAction::kVolumeMute,
    AcceleratorAction::kVolumeUp,
};

// Actions that are duplicated with browser shortcuts.
ASH_EXPORT inline constexpr std::array kActionsDuplicatedWithBrowser = {
    // clang-format off
    AcceleratorAction::kNewWindow,
    AcceleratorAction::kNewIncognitoWindow,
    AcceleratorAction::kRestoreTab,
    AcceleratorAction::kNewTab,
    AcceleratorAction::kToggleMultitaskMenu,
// clang-format on

// kOpenFeedbackPage has two accelerators in browser:
// 1: [alt shift i] guarded by GOOGLE_CHROME_BRANDING.
// 2: [search ctrl i] guarded by both GOOGLE_CHROME_BRANDING and IS_CHROMEOS.
// This file is built only for ash-chrome, so we only need to check BRANDING
// macro.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    AcceleratorAction::kOpenFeedbackPage,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

// Actions that are interceptable by browser.
// These actions are ash's shortcuts, but they are sent to the browser
// once in order to make it interceptable by webpage/apps.
ASH_EXPORT inline constexpr std::array kActionsInterceptableByBrowser = {
    AcceleratorAction::kShowTaskManager,
    AcceleratorAction::kOpenGetHelp,
    AcceleratorAction::kMinimizeTopWindowOnBack,
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
