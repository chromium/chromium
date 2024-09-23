// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_

#include <cstddef>

namespace arc::input_overlay {

// About Json strings.
constexpr char kMouseAction[] = "mouse_action";
constexpr char kPrimaryClick[] = "primary_click";
constexpr char kSecondaryClick[] = "secondary_click";
constexpr char kHoverMove[] = "hover_move";
constexpr char kPrimaryDragMove[] = "primary_drag_move";
constexpr char kSecondaryDragMove[] = "secondary_drag_move";

// System version for AlphaV2.
constexpr char kSystemVersionAlphaV2[] = "0.2";

// The coordinates number, including Axis x and y.
constexpr int kAxisSize = 2;

// Total key size for ActionMoveKey.
constexpr size_t kActionMoveKeysSize = 4;

// Maximum of actions size.
inline constexpr size_t kMaxActionCount = 50;

constexpr char16_t kUnknownBind[] = u"?";

// Directions from up, left, down, right.
constexpr int kDirection[kActionMoveKeysSize][kAxisSize] = {{0, -1},
                                                            {-1, 0},
                                                            {0, 1},
                                                            {1, 0}};

// From ActionTap AlphaV2 design. There is the label offset to touch point in
// the edit mode.
constexpr int kOffsetToTouchPoint = -1;  // 2 - 3(kDotOutsideStrokeThickness)

// The space between EditingList and main window when EditingList is outside of
// the game window.
constexpr int kEditingListSpaceBetweenMainWindow = 5;
// The offset from the game window content when EditingList is inside of the
// game window.
constexpr int kEditingListOffsetInsideMainWindow = 24;
// The offset from the action view list item to the editing list border.
constexpr int kEditingListInsideBorderInsets = 16;

// Width of `EditingList`.
constexpr int kEditingListWidth = 296;
// Width of `ButtonOptionsMenu` minus the triangle height.
constexpr int kButtonOptionsMenuWidth = 296;

// Horizontal order inset for `ArrowContainer` and its children.
constexpr int kArrowContainerHorizontalBorderInset = 16;

// Arrow key move distance per key press event.
inline constexpr int kArrowKeyMoveDistance = 2;

// Display mode for display overlay.
enum class DisplayMode {
  kNone,
  // Display overlay can receive events but action labels can't be focused.
  // It shows educational dialog.
  // TODO(b/253646354): This will be removed when removing the Beta flag.
  kEducation,
  // Display overlay can't receive any events. It shows input mappings as in
  // view mode and menu anchor.
  kView,
  // Display overlay can receive events and action labels can be focused. It
  // shows input mapping in edit mode.
  kEdit,
  // Display overlay can receive events. This is the mode before entering into
  // `kMenu`.
  // TODO(b/253646354): This will be removed when removing the Beta flag.
  kPreMenu,
  // Display overlay can receive events but action labels can't be focused.
  // It shows expanded menu and input mapping as in view mode.
  // TODO(b/253646354): This will be removed when removing the Beta flag.
  kMenu,

  // Below are related to edit for `ActionView`.
  // Edit mode when action is assigned a pending input binding.
  // TODO(b/253646354): This will be removed when removing the Beta flag.
  kEditedSuccess,
  // Edit mode when an action is removed the input binding.
  // TODO(b/253646354): This will be removed when removing the Beta flag.
  kEditedUnbound,
  // Edit mode when a wrong/unsupported input is trying to bind.
  // TODO(b/253646354): This will be removed when removing the Beta flag.
  kEditedError,
  // TODO(b/253646354): This will be removed when removing the Beta flag.
  // Restore mode when restoring the default input bindings.
  kRestore,
};

// Binding options for different ui display stages.
enum class BindingOption {
  // Current input binding in active.
  kCurrent,
  // Original default input binding provided by Google.
  kOriginal,
  // Pending input binding generated during the binding editing before it is
  // saved.
  kPending,
};

// Message types for UI displaying different types of messages.
enum class MessageType {
  // `kInfo` is the type for info message.
  kInfo,
  // `kError` is the type for error message.
  kError,
  // `kInfoLabelFocus` is the type for info message when the `ActionLabel` is
  // focused.
  kInfoLabelFocus,
};

// Position type enum.
enum class PositionType {
  // Default position type.
  kDefault = 0,
  // Dependent position type which x or y value depend on the other one.
  kDependent = 1,
};

// The label position related to touch center for ActionTap.
enum class TapLabelPosition {
  // Top-left of touch point. Starts to use in AlphaV2.
  kTopLeft = 0,
  // Top-right of touch point. Starts to use in AlphaV2.
  kTopRight = 1,
  // Bottom-left of touch point. Starts to use in Alpha.
  kBottomLeft = 2,
  // Bottom-right of touch point. Starts to use in Alpha.
  kBottomRight = 3,
  // Undefined label position. Starts to use in AlphaV2.
  kNone = 4,
};

// The UI state related to user operations.
enum class UIState {
  // UI is not hovered or dragged.
  kDefault = 0,
  // UI is under dragging.
  kDrag,
  // UI is mouse hovered.
  kHover,
};

// These values are about how the reposition is achieved for the metrics record.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RepositionType {
  kTouchscreenDragRepostion = 0,
  kMouseDragRepostion = 1,
  kKeyboardArrowKeyReposition = 2,
  kMaxValue = kKeyboardArrowKeyReposition
};

// This is about the window state types when recording metrics data for user UI
// reposition for the metrics record. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class InputOverlayWindowStateType {
  kInvalid = 0,
  kNormal = 1,
  kMaximized = 2,
  kFullscreen = 3,
  kSnapped = 4,
  kMaxValue = kSnapped
};

// This is about the four directions of the ActionMove.
enum class Direction : size_t {
  kUp = 0,
  kLeft = 1,
  kDown = 2,
  kRight = 3,
  kMaxValue = kRight
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_
