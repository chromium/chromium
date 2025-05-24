// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_

#include <cstddef>

namespace arc::input_overlay {

// About Json strings.
inline constexpr char kMouseAction[] = "mouse_action";
inline constexpr char kPrimaryClick[] = "primary_click";
inline constexpr char kSecondaryClick[] = "secondary_click";
inline constexpr char kHoverMove[] = "hover_move";
inline constexpr char kPrimaryDragMove[] = "primary_drag_move";
inline constexpr char kSecondaryDragMove[] = "secondary_drag_move";

// System version for AlphaV2+.
inline constexpr char kSystemVersionAlphaV2Plus[] = "0.2";

// The coordinates number, including Axis x and y.
inline constexpr int kAxisSize = 2;

// Total key size for ActionMoveKey.
inline constexpr size_t kActionMoveKeysSize = 4;

// Maximum of actions size.
inline constexpr size_t kMaxActionCount = 50;

inline constexpr char16_t kUnknownBind[] = u"?";

// Directions from up, left, down, right.
inline constexpr int kDirection[kActionMoveKeysSize][kAxisSize] = {{0, -1},
                                                                   {-1, 0},
                                                                   {0, 1},
                                                                   {1, 0}};

// From ActionTap AlphaV2 design. There is the label offset to touch point in
// the edit mode.
//
// 2 - 3(kDotOutsideStrokeThickness)
inline constexpr int kOffsetToTouchPoint = -1;

// The space between EditingList and main window when EditingList is outside of
// the game window.
inline constexpr int kEditingListSpaceBetweenMainWindow = 5;
// The offset from the game window content when EditingList is inside of the
// game window.
inline constexpr int kEditingListOffsetInsideMainWindow = 24;
// The offset from the action view list item to the editing list border.
inline constexpr int kEditingListInsideBorderInsets = 16;

// Width of `EditingList`.
inline constexpr int kEditingListWidth = 296;
// Width of `ButtonOptionsMenu` minus the triangle height.
inline constexpr int kButtonOptionsMenuWidth = 296;

// Horizontal order inset for `ArrowContainer` and its children.
inline constexpr int kArrowContainerHorizontalBorderInset = 16;

// Arrow key move distance per key press event.
inline constexpr int kArrowKeyMoveDistance = 2;

// Display mode for display overlay.
enum class DisplayMode {
  kNone,
  // Display overlay can't receive any events. It shows input mappings as in
  // view mode and menu anchor.
  kView,
  // Display overlay can receive events and action labels can be focused. It
  // shows input mapping in edit mode.
  kEdit,
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
