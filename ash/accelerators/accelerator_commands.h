// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
#define ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_

#include "ash/ash_export.h"

// This file contains implementations of commands that are bound to keyboard
// shortcuts in Ash or in the embedding application (e.g. Chrome).
//
// Keep the functions in this file in alphabetical order.
namespace ash {
namespace accelerators {

// Logs a dump of CalendarModel internal data.
ASH_EXPORT void DumpCalendarModel();

// Cycle backwards in the MRU window list. Usually Alt-Shift-Tab.
ASH_EXPORT void CycleBackwardMru();

// Cycle forwards in the MRU window list. Usually Alt-Tab.
ASH_EXPORT void CycleForwardMru();

// Disable caps-lock.
ASH_EXPORT void DisableCapsLock();

// Focus the camera preview if it is present.
ASH_EXPORT void FocusCameraPreview();

// Focus the PiP window if it is present.
ASH_EXPORT void FocusPip();

// Focus the shelf.
ASH_EXPORT void FocusShelf();

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

// Toggles the fullscreen state. The behavior can be overridden
// by WindowStateDelegate::ToggleFullscreen().
ASH_EXPORT void ToggleFullscreen();

// Toggles to show/close the Ime Menu.
ASH_EXPORT void ToggleImeMenuBubble();

// Toggles the Projector annotation tray UI and marker enabled state.
ASH_EXPORT void ToggleProjectorMarker();

// Toggle keyboard backlight.
ASH_EXPORT void ToggleKeyboardBacklight();

// Toggles the maxmized state. If the window is in fulllscreen, it exits
// fullscreen mode.
ASH_EXPORT void ToggleMaximized();

// Minimizes the active window, if present. If no windows are active, restores
// the first unminimized window. Returns true if a window was minimized or
// restored.
ASH_EXPORT bool ToggleMinimized();

// Toggles on/off the electronic privacy screen.
ASH_EXPORT void TogglePrivacyScreen();

// Toggles the resize lock mode menu for a focused ARC++ resize-locked app if
// present.
ASH_EXPORT void ToggleResizeLockMenu();

// Toggles the unified desktop mode which allows a window to span multiple
// displays.
ASH_EXPORT void ToggleUnifiedDesktop();

// If a window is pinned (aka forced fullscreen), exit from pinned mode.
ASH_EXPORT void UnpinWindow();

// Volume down.
ASH_EXPORT void VolumeDown();

// Volume mute.
ASH_EXPORT void VolumeMute();

// Volume up.
ASH_EXPORT void VolumeUp();

// Change the display zooming up or down.
ASH_EXPORT bool ZoomDisplay(bool up);

}  // namespace accelerators
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
