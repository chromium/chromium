// Copyright 2013 The Chromium Authors. All rights reserved.
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

// Cycle backwards in the MRU window list. Usually Alt-Shift-Tab.
ASH_EXPORT void CycleBackwardMru();

// Cycle forwards in the MRU window list. Usually Alt-Tab.
ASH_EXPORT void CycleForwardMru();

// Disable caps-lock.
ASH_EXPORT void DisableCapsLock();

// Focus the PiP window if it is present.
ASH_EXPORT void FocusPip();

// Launch the nth(0-7) app on the shelf.
ASH_EXPORT void LaunchAppN(int n);

// Launch the right-most app on the shelf.
ASH_EXPORT void LaunchLastApp();

// Lock the screen.
ASH_EXPORT void LockScreen();

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

// Reset the display zooming to the default state.
ASH_EXPORT void ResetDisplayZoom();

// Restore the last closed tab in the browser.
ASH_EXPORT void RestoreTab();

// Change primary display to the secondary display next to current primary
// display
ASH_EXPORT void ShiftPrimaryDisplay();

// Toogles to show and hide the calendar widget.
ASH_EXPORT void ToggleCalendar();

// Toggles the fullscreen state. The behavior can be overridden
// by WindowStateDelegate::ToggleFullscreen().
ASH_EXPORT void ToggleFullscreen();

// Toggle keyboard backlight.
ASH_EXPORT void ToggleKeyboardBacklight();

// Toggles the maxmized state. If the window is in fulllscreen, it exits
// fullscreen mode.
ASH_EXPORT void ToggleMaximized();

// Minimizes the active window, if present. If no windows are active, restores
// the first unminimized window. Returns true if a window was minimized or
// restored.
ASH_EXPORT bool ToggleMinimized();

// Toggles the resize lock mode menu for a focused ARC++ resize-locked app if
// present.
ASH_EXPORT void ToggleResizeLockMenu();

// If a window is pinned (aka forced fullscreen), exit from pinned mode.
ASH_EXPORT void UnpinWindow();

// Change the display zooming up or down.
ASH_EXPORT bool ZoomDisplay(bool up);

}  // namespace accelerators
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
