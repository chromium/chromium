// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

class PointerHighlightLayer;
class KeyComboView;
class VideoRecordingWatcher;

using MouseHighlightLayers =
    std::vector<std::unique_ptr<PointerHighlightLayer>>;

using TouchHighlightLayersMap =
    base::flat_map<ui::PointerId, std::unique_ptr<PointerHighlightLayer>>;

// Observes and decides whether to show clicks (or taps) highlights and a helper
// widget that represents the currently pressed key combination or not. The key
// combination will be used to construct or modify the `KeyComboViewer`. The
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

  const views::Widget* key_combo_widget() const {
    return key_combo_widget_.get();
  }

  // Decides whether to show a helper widget for the `event` or not.
  void OnKeyEvent(ui::KeyEvent* event);

  // Creates a new highlight layer each time it gets called and performs the
  // grow-and-fade-out animation on it.
  void PerformMousePressAnimation(const gfx::PointF& event_location_in_window);

  // Refreshes the bounds of `key_combo_widget_`.
  void RefreshBounds();

  // Decides whether to show the highlight for the touch event or not.
  void OnTouchEvent(ui::EventType event_type,
                    ui::PointerId pointer_id,
                    const gfx::PointF& event_location_in_window);

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}

 private:
  friend class CaptureModeDemoToolsTestApi;

  void OnKeyUpEvent(ui::KeyEvent* event);
  void OnKeyDownEvent(ui::KeyEvent* event);

  // Refreshes the state of the `key_combo_view_` based on the current state of
  // `modifiers_` and `last_non_modifier_key_`.
  void RefreshKeyComboViewer();

  gfx::Rect CalculateKeyComboWidgetBounds() const;

  // Returns true if there is no modifier keys pressed and the non-modifier key
  // can not be displayed independently.
  bool ShouldResetKeyComboWidget() const;

  // Resets the `key_combo_widget_` when the `key_up_refresh_timer_` expires.
  void AnimateToResetKeyComboWidget();

  void UpdateTextInputType(const ui::TextInputClient* client);

  // Called when the mouse highlight animation ends to remove the corresponding
  // pointer highlight from the `mouse_highlight_layers_`.
  void OnMouseHighlightAnimationEnded(
      PointerHighlightLayer* pointer_highlight_layer_ptr);

  // Creates a new highlight layer each time it gets called and performs the
  // grow animation on it.
  void OnTouchDown(const ui::PointerId& pointer_id,
                   const gfx::PointF& event_location_in_window);

  // Performs the grow-and-fade-out animation on an existing highlight layer
  // that corresponds to the given `pointer_id`.
  void OnTouchUp(const ui::PointerId& pointer_id,
                 const gfx::PointF& event_location_in_window);

  // Sets the bounds of the touch highlight layer that corresponds to the
  // `pointer_id` based on the `event_location_in_window` of the touch event
  // when it gets called on touch dragged.
  void OnTouchDragged(const ui::PointerId& pointer_id,
                      const gfx::PointF& event_location_in_window);

  const raw_ptr<VideoRecordingWatcher> video_recording_watcher_;
  views::UniqueWidgetPtr key_combo_widget_;
  raw_ptr<KeyComboView> key_combo_view_ = nullptr;

  // The state of the modifier keys i.e. Shift/Ctrl/Alt/Launcher keys.
  int modifiers_ = 0;

  // The most recently pressed non-modifier key.
  ui::KeyboardCode last_non_modifier_key_ = ui::VKEY_UNKNOWN;

  // True if the cursor and focus is currently in a text input
  // field, false otherwise.
  bool in_text_input_ = false;

  // Used to hold on `RefreshKeyComboViewer`. The key combo widget will be
  // scheduled to hide after `capture_mode::kRefreshKeyComboWidgetLongDelay`
  // when a key up event is received, and the remaining pressed keys are no
  // longer displayable, e.g. for a key combo Ctrl
  // + C, after Ctrl is released, the remaining C is no longer displayable on
  // its own as a complete key combo, so the widget will be scheduled to hide
  // after `capture_mode::kRefreshKeyComboWidgetLongDelay`. Otherwise
  // `capture_mode::kRefreshKeyComboWidgetShortDelay` will be used as the
  // threshold duration to decide the user intention on whether to do an update
  // or release multiple keys at one time.
  base::OneShotTimer key_up_refresh_timer_;

  // Contains all the mouse highlight layers that are being animated.
  MouseHighlightLayers mouse_highlight_layers_;

  // Maps the PointerHighlightLayer of the touch event by the pointer id as the
  // key.
  TouchHighlightLayersMap touch_pointer_id_to_highlight_layer_map_;

  // If set, it will be called when the mouse highlight animation is completed.
  base::OnceClosure on_mouse_highlight_animation_ended_callback_for_test_;

  base::WeakPtrFactory<CaptureModeDemoToolsController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_CONTROLLER_H_
