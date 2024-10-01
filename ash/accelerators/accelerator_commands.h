// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
#define ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_

#include "ash/app_list/app_list_metrics.h"
#include "ash/ash_export.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/session/session_types.h"

// This file contains implementations of commands that are bound to keyboard
// shortcuts in Ash or in the embedding application (e.g. Chrome).
//
// Keep the functions in this file in alphabetical order.
namespace ash {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Captures usage of Alt+[ and Alt+].
enum class WindowSnapAcceleratorAction {
  kCycleLeftSnapInClamshellNoOverview = 0,
  kCycleLeftSnapInClamshellOverview = 1,
  kCycleLeftSnapInTablet = 2,
  kCycleRightSnapInClamshellNoOverview = 3,
  kCycleRightSnapInClamshellOverview = 4,
  kCycleRightSnapInTablet = 5,
  kMaxValue = kCycleRightSnapInTablet,
};

// UMA accessibility histogram names.
ASH_EXPORT extern const char kAccessibilityHighContrastShortcut[];
ASH_EXPORT extern const char kAccessibilitySpokenFeedbackShortcut[];
ASH_EXPORT extern const char kAccessibilityScreenMagnifierShortcut[];
ASH_EXPORT extern const char kAccessibilityDockedMagnifierShortcut[];

// Name of histogram corresponding to |WindowSnapAcceleratorAction|.
ASH_EXPORT extern const char kAccelWindowSnap[];

namespace accelerators {

//////////////////////////////////////////////////////////////////////////////
// CanFoo() functions:
// True should be returned if running |action| does something. Otherwise,
// false should be returned to give the web contents a chance at handling the
// accelerator.
// Note: These functions should be independent and not depend on
// ui::Accelerator.

ASH_EXPORT bool CanActivateTouchHud();

ASH_EXPORT bool CanCreateNewIncognitoWindow();

ASH_EXPORT bool CanCycleInputMethod();

ASH_EXPORT bool CanCycleMru();

ASH_EXPORT bool CanCycleSameAppWindows();

ASH_EXPORT bool CanCycleUser();

ASH_EXPORT bool CanFindPipWidget();

ASH_EXPORT bool CanFocusCameraPreview();

ASH_EXPORT bool CanLock();

ASH_EXPORT bool CanMoveActiveWindowBetweenDisplays();

ASH_EXPORT bool CanCreateSnapGroup();

ASH_EXPORT void CreateSnapGroup();

ASH_EXPORT bool CanMinimizeTopWindowOnBack();

ASH_EXPORT bool CanPerformMagnifierZoom();

ASH_EXPORT bool CanScreenshot(bool take_screenshot);

ASH_EXPORT bool CanShowStylusTools();

ASH_EXPORT bool CanStopScreenRecording();

ASH_EXPORT bool CanSwapPrimaryDisplay();

ASH_EXPORT bool CanTilingWindowResize();

ASH_EXPORT bool CanToggleCalendar();

ASH_EXPORT bool CanEnableOrToggleDictation();

ASH_EXPORT bool CanToggleFloatingWindow();

ASH_EXPORT bool CanToggleGameDashboard();

ASH_EXPORT bool CanToggleMultitaskMenu();

ASH_EXPORT bool CanToggleOverview();

ASH_EXPORT bool CanTogglePicker();

ASH_EXPORT bool CanTogglePrivacyScreen();

ASH_EXPORT bool CanToggleProjectorMarker();

ASH_EXPORT bool CanToggleResizeLockMenu();

ASH_EXPORT bool CanUnpinWindow();

ASH_EXPORT bool CanWindowSnap();

ASH_EXPORT bool CanResizePipWindow();

//////////////////////////////////////////////////////////////////////////////
// Accelerator commands.
// Note: These functions should be independent and not depend on ui::Accelerator

// Runs the assigned accessibility action.
ASH_EXPORT void AccessibilityAction();

// Activates desk on the left/right.
ASH_EXPORT void ActivateDesk(bool activate_left);

// Activates desk 1 to 8.
ASH_EXPORT void ActivateDeskAtIndex(AcceleratorAction action);

// Changes the scale of the active magnifier.
ASH_EXPORT void ActiveMagnifierZoom(int delta_index);

// Brightness down.
ASH_EXPORT void BrightnessDown();

// Brightness up.
ASH_EXPORT void BrightnessUp();

// Cycles windows of the same app/all windows backwards in the MRU window list.
ASH_EXPORT void CycleBackwardMru(bool same_app_only);

// Cycles windows of the same app/all windows forwards in the MRU window list.
ASH_EXPORT void CycleForwardMru(bool same_app_only);

// Switches to next/previous user.
ASH_EXPORT void CycleUser(CycleUserDirection direction);

// Disables caps-lock.
ASH_EXPORT void DisableCapsLock();

// Fingerprint sensor touched with finger finger_id finger_id can be 1,2 or 3.
// 3 different id is enough to cover all testing scenarios and it's better to
// minimize the number of used dev keyboard shortcuts.
ASH_EXPORT void TouchFingerprintSensor(int finger_id);

// Focuses the camera preview if it is present.
ASH_EXPORT void FocusCameraPreview();

// Focuses the PiP window if it is present.
ASH_EXPORT void FocusPip();

// Focuses the shelf.
ASH_EXPORT void FocusShelf();

// Dims keyboard.
ASH_EXPORT void KeyboardBrightnessDown();

// Makes keyboard brighter
ASH_EXPORT void KeyboardBrightnessUp();

// Launches the nth(0-7) app on the shelf.
ASH_EXPORT void LaunchAppN(int n);

// Launches the right-most app on the shelf.
ASH_EXPORT void LaunchLastApp();

// Presses lock button.
ASH_EXPORT void LockPressed(bool pressed);

// Locks the screen.
ASH_EXPORT void LockScreen();

// Takes partial screenshot/recording.
ASH_EXPORT void MaybeTakePartialScreenshot();

// Takes window screenshot/recording.
ASH_EXPORT void MaybeTakeWindowScreenshot();

// Fast-forwards playing media.
ASH_EXPORT void MediaFastForward();

// Goes to the next media track.
ASH_EXPORT void MediaNextTrack();

// Pauses media.
ASH_EXPORT void MediaPause();

// Plays media.
ASH_EXPORT void MediaPlay();

// Toggles pause or play on media.
ASH_EXPORT void MediaPlayPause();

// Goes to the previous media track.
ASH_EXPORT void MediaPrevTrack();

// Rewinds playing media.
ASH_EXPORT void MediaRewind();

// Stops playing media.
ASH_EXPORT void MediaStop();

// Moves active window between displays.
ASH_EXPORT void MoveActiveWindowBetweenDisplays();

// Toggles microphone mute.
ASH_EXPORT void MicrophoneMuteToggle();

// Moves active window to the desk on the left/right.
ASH_EXPORT void MoveActiveItem(bool going_left);

// Creates a new desk.
ASH_EXPORT void NewDesk();

// Opens a new incognito browser window.
ASH_EXPORT void NewIncognitoWindow();

// Opens a new tab.
ASH_EXPORT void NewTab();

// Opens a new browser window.
ASH_EXPORT void NewWindow();

// Opens the calculator app.
ASH_EXPORT void OpenCalculator();

// Opens Crosh.
ASH_EXPORT void OpenCrosh();

// Opens the diagnostics app.
ASH_EXPORT void OpenDiagnostics();

// Opens the feedback app.
ASH_EXPORT void OpenFeedbackPage();

// Opens the file manager app.
ASH_EXPORT void OpenFileManager();

// Opens the help/explore app.
ASH_EXPORT void OpenHelp();

// Resizes window as a tile.
ASH_EXPORT void PerformTilingWindowResize(AcceleratorAction action);

// Presses power button.
ASH_EXPORT void PowerPressed(bool pressed);

// Records when the user changes the output volume via keyboard to metrics.
ASH_EXPORT void RecordVolumeSource();

// Removes the current desk.
ASH_EXPORT void RemoveCurrentDesk();

// Resets the display zooming to the default state.
ASH_EXPORT void ResetDisplayZoom();

// Restores the last closed tab in the browser.
ASH_EXPORT void RestoreTab();

// Rotates the active window 90 degrees.
ASH_EXPORT void RotateActiveWindow();

// Rotates pane focus on next/previous pane.
ASH_EXPORT void RotatePaneFocus(FocusCycler::Direction direction);

// Rotates screen 90 degrees.
ASH_EXPORT void RotateScreen();

// Changes primary display to the secondary display next to current primary
// display
ASH_EXPORT void ShiftPrimaryDisplay();

// Opens Emoji Picker.
// `accelerator_timestamp` is the timestamp associated with the accelerator that
// triggered the emoji picker.
ASH_EXPORT void ShowEmojiPicker(base::TimeTicks accelerator_timestamp);

// Opens Shortcut Customization.
ASH_EXPORT void ShowShortcutCustomizationApp();

// Brings up task manager.
ASH_EXPORT void ShowTaskManager();

// Stops the capture mode recording.
ASH_EXPORT void StopScreenRecording();

// Puts device in sleep mode(suspend).
ASH_EXPORT void Suspend();

// Switches to next language.
ASH_EXPORT void SwitchToNextIme();

// Switches to the previous language.
ASH_EXPORT void SwitchToLastUsedIme(bool key_pressed);

// Takes screenshot.
ASH_EXPORT void TakeScreenshot(bool from_snapshot_key);

// Toggles app list.
ASH_EXPORT void ToggleAppList(AppListShowSource show_source,
                              base::TimeTicks event_time_stamp);

// Assigns active window to all desks.
ASH_EXPORT void ToggleAssignToAllDesk();

// Toggles Google assistant.
ASH_EXPORT void ToggleAssistant();

// Toogles to show and hide the calendar widget.
ASH_EXPORT void ToggleCalendar();

// Turns caps lock on and off.
ASH_EXPORT void ToggleCapsLock();

// Toggles the clipboard history.
ASH_EXPORT void ToggleClipboardHistory(bool is_plain_text_paste);

// Toggles Picker.
// `accelerator_timestamp` is the timestamp associated with the accelerator that
// triggered Picker.
ASH_EXPORT void TogglePicker(base::TimeTicks accelerator_timestamp);

// Enables Select to Speak if the feature is currently disabled. Does nothing if
// the feature is currently enabled.
ASH_EXPORT void EnableSelectToSpeak();

// Enables Dictation if the feature is currently disabled. Toggles (either
// starts or stops) Dictation if the feature is currently enabled.
ASH_EXPORT void EnableOrToggleDictation();

// Turns the docked magnifier on or off.
ASH_EXPORT void ToggleDockedMagnifier();

// Toggles the floating window.
ASH_EXPORT void ToggleFloating();

// Toggles the fullscreen state. The behavior can be overridden
// by WindowStateDelegate::ToggleFullscreen().
ASH_EXPORT void ToggleFullscreen();

// Turns the fullscreen magnifier mode on or off.
ASH_EXPORT void ToggleFullscreenMagnifier();

// Toggles the game dashboard on the current window.
ASH_EXPORT void ToggleGameDashboard();

// Turns the high contrast mode on or off.
ASH_EXPORT void ToggleHighContrast();

// Toggles to show/close the Ime Menu.
ASH_EXPORT void ToggleImeMenuBubble();

// Toggles keyboard backlight.
ASH_EXPORT void ToggleKeyboardBacklight();

// Toggles the maxmized state. If the window is in fulllscreen, it exits
// fullscreen mode.
ASH_EXPORT void ToggleMaximized();

// Turns the message center on or off.
ASH_EXPORT void ToggleMessageCenterBubble();

// Minimizes the active window, if present. If no windows are active, restores
// the first unminimized window. Returns true if a window was minimized or
// restored.
ASH_EXPORT bool ToggleMinimized();

// Turns on or off Mouse Keys.
ASH_EXPORT void ToggleMouseKeys();

// Minimizes the topmost unminimized snap groups. If there is no such snap
// group, restores the most recently used minimized snap group.
// TODO(b/333772909): Remove this API when the mojom conversion is disabled for
// deprecated shortcuts.
ASH_EXPORT void ToggleSnapGroupsMinimize();

// Turns the mirror mode on or off.
ASH_EXPORT void ToggleMirrorMode();

// Toggles the multitask menu.
ASH_EXPORT void ToggleMultitaskMenu();

// Turns the overview mode on or off.
ASH_EXPORT void ToggleOverview();

// Toggles on/off the electronic privacy screen.
ASH_EXPORT void TogglePrivacyScreen();

// Toggles the Projector annotation tray UI and marker enabled state.
ASH_EXPORT void ToggleProjectorMarker();

// Toggles the resize lock mode menu for a focused ARC++ resize-locked app if
// present.
ASH_EXPORT void ToggleResizeLockMenu();

// Turns ChromeVox (spoken feedback) on or off.
ASH_EXPORT void ToggleSpokenFeedback();

// Toggles the stylus tools bubble on or off.
ASH_EXPORT void ToggleStylusTools();

// Turns the system tray on or off.
ASH_EXPORT void ToggleSystemTrayBubble();

// Turns the wifi on or off.
ASH_EXPORT void ToggleWifi();

// Toggles the unified desktop mode which allows a window to span multiple
// displays.
ASH_EXPORT void ToggleUnifiedDesktop();

// Minimizes the top window on the back.
ASH_EXPORT void TopWindowMinimizeOnBack();

// Clears the touch hud.
ASH_EXPORT void TouchHudClear();

// Changes the touch hud mode.
ASH_EXPORT void TouchHudModeChange();

// If a window is pinned (aka forced fullscreen), exits from pinned mode.
ASH_EXPORT void UnpinWindow();

// Volume down.
ASH_EXPORT void VolumeDown();

// Volume mute.
ASH_EXPORT void VolumeMute();

// Volume mute toggle.
ASH_EXPORT void VolumeMuteToggle();

// Volume up.
ASH_EXPORT void VolumeUp();

// Minimizes the window.
ASH_EXPORT void WindowMinimize();

// Snaps window to the left/right.
ASH_EXPORT void WindowSnap(AcceleratorAction action);

// Changes the display zooming up or down.
ASH_EXPORT bool ZoomDisplay(bool up);

// Resize the pip window.
ASH_EXPORT void ResizePipWindow();

}  // namespace accelerators
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
