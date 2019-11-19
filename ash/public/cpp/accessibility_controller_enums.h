// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_ENUMS_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_ENUMS_H_

namespace ash {

// Alert sent to the accessibility api.
enum class AccessibilityAlert {
  // Default value, indicates no accessibility alert.
  NONE,

  // When caps lock is turned on.
  CAPS_ON,

  // When caps lock is turned off.
  CAPS_OFF,

  // When screen is turned on by tablet power button.
  SCREEN_ON,

  // When screen is turned off by tablet power button.
  SCREEN_OFF,

  // When window moved to another display by accelerators.
  WINDOW_MOVED_TO_ANOTHER_DISPLAY,

  // When the user attempts a keyboard command that requires a window to work,
  // and none is available.
  WINDOW_NEEDED,

  // When the user enters window overview mode.
  WINDOW_OVERVIEW_MODE_ENTERED,

  // When workspace state just changed to WorkspaceWindowState::kFullscreen.
  WORKSPACE_FULLSCREEN_STATE_ENTERED,

  // When workspace state just changed from WorkspaceWindowState::kFullscreen.
  // to others.
  WORKSPACE_FULLSCREEN_STATE_EXITED
};

enum class AccessibilityPanelState {
  // Window bounds are set explicitly.
  BOUNDED,

  // Width of panel matches screen width, y_coord and height are set by bounds.
  FULL_WIDTH,

  // Panel occupies the full screen. Bounds are ignored.
  FULLSCREEN
};

// These values are persisted to logs and should not be renumbered or re-used.
// See tools/metrics/histograms/enums.xml.
enum class DictationToggleSource {
  // Toggled by the keyboard command (Search + D).
  kKeyboard,

  // Toggled by the dictation button in the tray.
  kButton,

  // Switch Access context menu button.
  kSwitchAccess,

  // Chromevox chrome extension.
  kChromevox,

  kMaxValue = kChromevox
};

enum class SelectToSpeakState {
  // Select to Speak is not actively selecting text or speaking.
  kSelectToSpeakStateInactive,

  // Select to Speak is being used to actively select a new region. Note that
  // it might also be speaking, but the selecting state takes precedence.
  kSelectToSpeakStateSelecting,

  // Select to Speak is currently speaking.
  kSelectToSpeakStateSpeaking,
};

enum class SwitchAccessCommand {
  // Do not perform a command.
  kNone,
  // Command to select the focused element.
  kSelect,
  // Command to move focus to the next element.
  kNext,
  // Command to move focus to the previous element.
  kPrevious,
};

// The type of mouse event the Automatic Clicks feature should perform when
// dwelling. These values are written to prefs and correspond to
// AutoclickActionType in enums.xml, so should not be changed. New values
// should be added at the end.
enum class AutoclickEventType {
  // Perform a left click.
  kLeftClick = 0,

  // Perform a right click.
  kRightClick = 1,

  // Perform a drag and drop, i.e. click down at the first dwell, and up at the
  // second dwell.
  kDragAndDrop = 2,

  // Perform a double-click.
  kDoubleClick = 3,

  // A non-action, i.e. nothing will happen at the end of the dwell time.
  kNoAction = 4,

  // A mousewheel scroll action. An additional menu will be shown for the user
  // to pick whether they want to scroll up/down/left/right.
  kScroll = 5,

  kMaxValue = kScroll
};

// The Automatic Clicks feature's on-screen menu display location. These values
// are written to prefs so they should not be changed. New values should be
// added at the end.
enum class AutoclickMenuPosition {
  // The bottom right of the screen.
  kBottomRight,

  // The bottom left of the screen.
  kBottomLeft,

  // The top left of the screen.
  kTopLeft,

  // The top right of the screen.
  kTopRight,

  // The default position. This will be either the bottom right in LTR languages
  // or the bottom right in RTL languages. Once the user explicitly picks
  // a position it will no longer change with language direction.
  kSystemDefault,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_ENUMS_H_
