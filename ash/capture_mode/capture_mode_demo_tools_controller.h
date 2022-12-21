// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

class PointerHighlightLayer;
class KeyComboView;
class VideoRecordingWatcher;

using MouseHighlightLayers =
    std::vector<std::unique_ptr<PointerHighlightLayer>>;

// Observes and decides whether to show a helper widget representing the
// currently pressed key combination or not. The key combination will be used to
// construct or modify the `KeyComboViewer`. The
// `CaptureModeDemoToolsController` will only be available during video
// recording and has to be explicitly enabled by the user.
class CaptureModeDemoToolsController : public ui::InputMethodObserver {
 public:
  explicit CaptureModeDemoToolsController(
      VideoRecordingWatcher* video_recording_watcher);
  CaptureModeDemoToolsController(const CaptureModeDemoToolsController&) =
      delete;
  CaptureModeDemoToolsController& operator=(
      const CaptureModeDemoToolsController&) = delete;
  ~CaptureModeDemoToolsController() override;

  // Decides whether to show a helper widget for the `event` or not.
  void OnKeyEvent(ui::KeyEvent* event);

  // Creates a new highlight layer each time it gets called and performs the
  // grow-and-fade-out animation on it.
  void PerformMousePressAnimation(const gfx::PointF& event_location_in_window);

  // Refreshes the bounds of the key combo viewer.
  void RefreshBounds();

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}

  const MouseHighlightLayers& mouse_highlight_layers_for_testing() const {
    return mouse_highlight_layers_;
  }

 private:
  friend class CaptureModeDemoToolsTestApi;

  void OnKeyUpEvent(ui::KeyEvent* event);
  void OnKeyDownEvent(ui::KeyEvent* event);

  // Refreshes the state of the `key_combo_view_` based on the current state of
  // `modifiers_` and `last_non_modifier_key_`.
  void RefreshKeyComboViewer();

  gfx::Rect CalculateBounds() const;

  // Resets the `demo_tools_widget_` when the `hide_timer_` expires.
  void AnimateToResetTheWidget();

  void UpdateTextInputType(const ui::TextInputClient* client);

  // Called when the mouse highlight animation ends to remove the corresponding
  // pointer highlight from the `mouse_highlight_layers_`.
  void OnMouseHighlightAnimationEnded(
      PointerHighlightLayer* pointer_highlight_layer_ptr);

  VideoRecordingWatcher* const video_recording_watcher_;
  views::UniqueWidgetPtr demo_tools_widget_;
  KeyComboView* key_combo_view_ = nullptr;

  // The state of the modifier keys i.e. Shift/Ctrl/Alt/Launcher keys.
  int modifiers_ = 0;

  // The most recently pressed non-modifier key.
  ui::KeyboardCode last_non_modifier_key_ = ui::VKEY_UNKNOWN;

  // True if the cursor and focus is currently in a password text input
  // field, false otherwise.
  bool in_password_text_input_ = false;

  // Starts on key up of the last non-modifier key and the `key_combo_view_`
  // will disappear when it expires.
  base::OneShotTimer hide_timer_;

  // Contains all the mouse highlight layers that are being animated.
  MouseHighlightLayers mouse_highlight_layers_;

  // If set, it will be called when the mouse highlight animation is completed.
  base::OnceClosure on_mouse_highlight_animation_ended_callback_for_test_;

  base::WeakPtrFactory<CaptureModeDemoToolsController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_