// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_

namespace arc::input_overlay {

// Display mode for display overlay.
enum class DisplayMode {
  kNone,
  // Display overlay can receive events but action labels can't be focused.
  // It shows educational dialog.
  kEducation,
  // Display overlay can't receive any events. It shows input mappings as in
  // view mode and menu anchor.
  kView,
  // Display overlay can receive events and action labels can be focused. It
  // shows input mapping in edit mode.
  kEdit,
  // Display overlay can receive events. This is the mode before entering into
  // |kMenu|.
  kPreMenu,
  // Display overlay can receive events but action labels can't be focused.
  // It shows expanded menu and input mapping as in view mode.
  kMenu,

  // Below are related to edit for |ActionView|.
  // Edit mode when action is assigned a pending input binding.
  kEditedSuccess,
  // Edit mode when an action is removed the input binding.
  kEditedUnbound,
  // Edit mode when a wrong/unsupported input is trying to bind.
  kEditedError,
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
  // |kInfo| is the type for info message.
  kInfo,
  // |kError| is the type for error message.
  kError,
  // |kInfoLabelFocus| is the type for info message when the |ActionLabel| is
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

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_CONSTANTS_H_
