// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_

#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

// Observes and decides whether to show a helper widget representing the
// currently pressed key combination or not. The key combination will be used to
// construct or modify the `KeyComboViewer`. The
// `CaptureModeDemoToolsController` will only be available during video
// recording and has to be explicitly enabled by the user.
class CaptureModeDemoToolsController {
 public:
  CaptureModeDemoToolsController();
  CaptureModeDemoToolsController(const CaptureModeDemoToolsController&) =
      delete;
  CaptureModeDemoToolsController& operator=(
      const CaptureModeDemoToolsController&) = delete;
  ~CaptureModeDemoToolsController();

  // Decides whether to show a helper widget for the `event` or not.
  void OnKeyEvent(ui::KeyEvent* event);

  bool demo_tools_widget_for_testing() const { return demo_tools_widget_; }
  int modifiers_for_testing() const { return modifiers_; }
  ui::KeyboardCode last_non_modifier_key_for_testing() const {
    return last_non_modifier_key_;
  }

 private:
  void OnKeyUpEvent(ui::KeyEvent* event);
  void OnKeyDownEvent(ui::KeyEvent* event);

  // Refresh the state of the key combo viewer based on the current state of
  // `modifiers_` and `last_non_modifier_key_`.
  void RefreshKeyComboViewer();

  // TODO(https://crbug.com/1356362): Remove this and replace it with the actual
  // demo tools widget. This was added temporarily for testing purpose.
  bool demo_tools_widget_ = false;

  // The state of the modifier keys i.e. Shift/Ctrl/Alt/Launcher keys.
  int modifiers_ = 0;

  // The most recently pressed non-modifier key.
  ui::KeyboardCode last_non_modifier_key_ = ui::VKEY_UNKNOWN;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_