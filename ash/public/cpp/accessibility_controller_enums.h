// Copyright 2019 The Chromium Authors
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
  WORKSPACE_FULLSCREEN_STATE_EXITED,

  // When the user enters saved desks mode.
  SAVED_DESKS_MODE_ENTERED,

  // When the user enters faster split screen setup session.
  FASTER_SPLIT_SCREEN_SETUP,

  // When the user resizes a snap group via arrow keys.
  SNAP_GROUP_RESIZE_LEFT,
  SNAP_GROUP_RESIZE_RIGHT,
  SNAP_GROUP_RESIZE_UP,
  SNAP_GROUP_RESIZE_DOWN,

  // When the user creates a snap group.
  SNAP_GROUP_CREATION,
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

  // Accessibility Common chrome extension.
  kAccessibilityCommon,

  kMaxValue = kAccessibilityCommon
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SelectToSpeakPanelAction {
  // No action.
  kNone = 0,
  // Navigate to previous paragraph/block.
  kPreviousParagraph = 1,
  // Navigate to previous sentence.
  kPreviousSentence = 2,
  // Pause text-to-speech.
  kPause = 3,
  // Resumes text-to-speech.
  kResume = 4,
  // Navigate to next sentence.
  kNextSentence = 5,
  // Navigate to next paragraph/block.
  kNextParagraph = 6,
  // Exit Select-to-speak.
  kExit = 7,
  // Change reading speed.
  kChangeSpeed = 8,
  kMaxValue = kChangeSpeed,
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

enum class MagnifierCommand {
  // Stop moving magnifier viewport.
  kMoveStop,
  // Command to move magnifier viewport up.
  kMoveUp,
  // Command to move magnifier viewport down.
  kMoveDown,
  // Command to move magnifier viewport left.
  kMoveLeft,
  // Command to move magnifier viewport right.
  kMoveRight,
};

// The type of mouse event the Automatic Clicks feature should perform when
// dwelling. These values are written to prefs and correspond to
// AutoclickActionType in enums.xml, so should not be changed. New values
// should be added at the end.
//
// LINT.IfChange(AutoclickEventType)
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
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AutoclickActionType)

// Display location of the on-screen floating menus used by accessibility
// features(e.g. the Automatic Clicks) . These values are written to prefs so
// they should not be changed. New values should be added at the end.
enum class FloatingMenuPosition {
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

// Mouse following mode for magnifier. This indicates the way the magnified
// viewport follows the mouse as it moves across the screen. These values are
// written to prefs so they should not be changed. New values should be added at
// the end.
enum class MagnifierMouseFollowingMode {
  // Continuous following mode.
  kContinuous = 0,

  // Centered following mode.
  kCentered = 1,

  // Edge following mode.
  kEdge = 2,

  kMaxValue = kEdge
};

// The icon shown in the Dictation bubble UI. This enum should be kept in sync
// with chrome.accessibilityPrivate.DictationBubbleIconType.
enum class DictationBubbleIconType {
  kHidden,
  kStandby,
  kMacroSuccess,
  kMacroFail,
};

// Hints that can show up in the Dictation bubble UI. This enum should be kept
// in sync with chrome.accessibilityPrivate.DictationBubbleHintType.
enum class DictationBubbleHintType {
  kTrySaying,
  kType,
  kDelete,
  kSelectAll,
  kUndo,
  kHelp,
  kUnselect,
  kCopy,
};

// The types of notifications that can be shown by Dictation.
enum class DictationNotificationType {
  kAllDlcsDownloaded,
  kNoDlcsDownloaded,
  kOnlySodaDownloaded,
  kOnlyPumpkinDownloaded,
};

// The types of notifications that can be shown by FaceGaze.
enum class FaceGazeNotificationType {
  kDlcFailed,
  kDlcSucceeded,
};

// The types of accessibility-related toasts. This enum should be kept in sync
// with chrome.accessibilityPrivate.ToastType.
enum class AccessibilityToastType {
  kDictationMicMuted,
  kDictationNoFocusedTextField,
  kTrackpadDisabled,
};

// Dominant hand for mouse keys.  This determines which keys to use for the
// feature.
enum class MouseKeysDominantHand {
  kRightHandDominant = 0,
  kLeftHandDominant = 1,
  kMaxValue = kLeftHandDominant,
};

// The four directions for scrolling.
enum class AccessibilityScrollDirection {
  kUp,
  kDown,
  kLeft,
  kRight,
};

// The different modes in which the internal trackpad can be disabled.
// These values are written to prefs so should not be changed.
enum class DisableTrackpadMode {
  kNever = 0,
  kAlways = 1,
  kOnExternalMouseConnected = 2,
  kMaxValue = kOnExternalMouseConnected,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_CONTROLLER_ENUMS_H_
