// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
#define ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_

#include "ash/ash_export.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/session/session_types.h"

// This file contains implementations of commands that are bound to keyboard
// shortcuts in Ash or in the embedding application (e.g. Chrome).
//
// Keep the functions in this file in alphabetical order.
namespace ash {
namespace accelerators {

// Activate desk 1 to 8.
ASH_EXPORT void ActivateDeskAtIndex(AcceleratorAction action);

// Change the scale of the active magnifier.
ASH_EXPORT void ActiveMagnifierZoom(int delta_index);

// Brightness down.
ASH_EXPORT void BrightnessDown();

// Brightness up.
ASH_EXPORT void BrightnessUp();

// Logs a dump of CalendarModel internal data.
ASH_EXPORT void DumpCalendarModel();

// Cycle backwards in the MRU window list. Usually Alt-Shift-Tab.
ASH_EXPORT void CycleBackwardMru();

// Cycle forwards in the MRU window list. Usually Alt-Tab.
ASH_EXPORT void CycleForwardMru();

// Switch to next/previous user.
ASH_EXPORT void CycleUser(CycleUserDirection direction);

// Disable caps-lock.
ASH_EXPORT void DisableCapsLock();

// Focus the camera preview if it is present.
ASH_EXPORT void FocusCameraPreview();

// Focus the PiP window if it is present.
ASH_EXPORT void FocusPip();

// Focus the shelf.
ASH_EXPORT void FocusShelf();

// Dim keyboard.
ASH_EXPORT void KeyboardBrightnessDown();

// Make keyboard brighter
ASH_EXPORT void KeyboardBrightnessUp();

// Launch the nth(0-7) app on the shelf.
ASH_EXPORT void LaunchAppN(int n);

// Launch the right-most app on the shelf.
ASH_EXPORT void LaunchLastApp();

// Lock the screen.
ASH_EXPORT void LockScreen();

// Take partial screenshot/recording.
ASH_EXPORT void MaybeTakePartialScreenshot();

// Take window screenshot/recording.
ASH_EXPORT void MaybeTakeWindowScreenshot();

// Fast-forward playing media.
ASH_EXPORT void MediaFastForward();

// Go to the next media track.
ASH_EXPORT void MediaNextTrack();

// Pause media.
ASH_EXPORT void MediaPause();

// Play media.
ASH_EXPORT void MediaPlay();

// Toggle pause or play on media.
ASH_EXPORT void MediaPlayPause();

// To to the previous media track.
ASH_EXPORT void MediaPrevTrack();

// Rewind playing media.
ASH_EXPORT void MediaRewind();

// Stop playing media.
ASH_EXPORT void MediaStop();

// Toggle microphone mute.
ASH_EXPORT void MicrophoneMuteToggle();

// Create a new desk.
ASH_EXPORT void NewDesk();

// Open a new incognito browser window.
ASH_EXPORT void NewIncognitoWindow();

// Open a new browser window.
ASH_EXPORT void NewWindow();

// Open the calculator app.
ASH_EXPORT void OpenCalculator();

// Open Crosh.
ASH_EXPORT void OpenCrosh();

// Open the diagnostics app.
ASH_EXPORT void OpenDiagnostics();

// Open the feedback app.
ASH_EXPORT void OpenFeedbackPage();

// Open the file manager app.
ASH_EXPORT void OpenFileManager();

// Open the help/explore app.
ASH_EXPORT void OpenHelp();

// Remove the current desk.
ASH_EXPORT void RemoveCurrentDesk();

// Reset the display zooming to the default state.
ASH_EXPORT void ResetDisplayZoom();

// Restore the last closed tab in the browser.
ASH_EXPORT void RestoreTab();

// Rotate the active window 90 degrees.
ASH_EXPORT void RotateActiveWindow();

// Rotate pane focus on next/previous pane.
ASH_EXPORT void RotatePaneFocus(FocusCycler::Direction direction);

// Change primary display to the secondary display next to current primary
// display
ASH_EXPORT void ShiftPrimaryDisplay();

// Open Emoji Picker.
ASH_EXPORT void ShowEmojiPicker();

// See keyboard shortcut helper.
ASH_EXPORT void ShowKeyboardShortcutViewer();

// Show stylus tools.
ASH_EXPORT void ShowStylusTools();

// Bring up task manager.
ASH_EXPORT void ShowTaskManager();

// Put device in sleep mode(suspend).
ASH_EXPORT void Suspend();

// Assign active window to all desks.
ASH_EXPORT void ToggleAssignToAllDesk();

// Toogles to show and hide the calendar widget.
ASH_EXPORT void ToggleCalendar();

// Turn caps lock on and off.
ASH_EXPORT void ToggleCapsLock();

// Toggles the clipboard history.
ASH_EXPORT void ToggleClipboardHistory();

// Turn the dictation on or off.
ASH_EXPORT void ToggleDictation();

// Turn the docked magnifier on or off.
ASH_EXPORT void ToggleDockedMagnifier();

// Toggles the floating window.
ASH_EXPORT void ToggleFloating();

// Toggles the fullscreen state. The behavior can be overridden
// by WindowStateDelegate::ToggleFullscreen().
ASH_EXPORT void ToggleFullscreen();

// Turn the fullscreen magnifier mode on or off.
ASH_EXPORT void ToggleFullscreenMagnifier();

// Turn the high contrast mode on or off.
ASH_EXPORT void ToggleHighContrast();

// Toggles to show/close the Ime Menu.
ASH_EXPORT void ToggleImeMenuBubble();

// Toggle keyboard backlight.
ASH_EXPORT void ToggleKeyboardBacklight();

// Toggles the maxmized state. If the window is in fulllscreen, it exits
// fullscreen mode.
ASH_EXPORT void ToggleMaximized();

// Turn the message center on or off.
ASH_EXPORT void ToggleMessageCenterBubble();

// Minimizes the active window, if present. If no windows are active, restores
// the first unminimized window. Returns true if a window was minimized or
// restored.
ASH_EXPORT bool ToggleMinimized();

// Turn the mirror mode on or off.
ASH_EXPORT void ToggleMirrorMode();

// Turn the overview mode on or off.
ASH_EXPORT void ToggleOverview();

// Toggles on/off the electronic privacy screen.
ASH_EXPORT void TogglePrivacyScreen();

// Toggles the Projector annotation tray UI and marker enabled state.
ASH_EXPORT void ToggleProjectorMarker();

// Toggles the resize lock mode menu for a focused ARC++ resize-locked app if
// present.
ASH_EXPORT void ToggleResizeLockMenu();

// Turn ChromeVox (spoken feedback) on or off.
ASH_EXPORT void ToggleSpokenFeedback();

// Turn the system tray on or off.
ASH_EXPORT void ToggleSystemTrayBubble();

// Turn the wifi on or off.
ASH_EXPORT void ToggleWifi();

// Toggles the unified desktop mode which allows a window to span multiple
// displays.
ASH_EXPORT void ToggleUnifiedDesktop();

// Minimize the top window on the back.
ASH_EXPORT void TopWindowMinimizeOnBack();

// Clear the touch hud.
ASH_EXPORT void TouchHudClear();

// Change the touch hud mode.
ASH_EXPORT void TouchHudModeChange();

// If a window is pinned (aka forced fullscreen), exit from pinned mode.
ASH_EXPORT void UnpinWindow();

// Volume down.
ASH_EXPORT void VolumeDown();

// Volume mute.
ASH_EXPORT void VolumeMute();

// Volume up.
ASH_EXPORT void VolumeUp();

// Minimize the window.
ASH_EXPORT void WindowMinimize();

// Snap window to the left/right.
ASH_EXPORT void WindowSnap(AcceleratorAction action);

// Change the display zooming up or down.
ASH_EXPORT bool ZoomDisplay(bool up);

}  // namespace accelerators
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
