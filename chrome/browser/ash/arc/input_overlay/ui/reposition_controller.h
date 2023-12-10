// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_REPOSITION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_REPOSITION_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"

namespace views {
class View;
}  // namespace views

namespace arc::input_overlay {

// It handles view reposition by mouse and touchscreen drag, and keyboard arrow
// keys and the view is always bounded inside of its parent. Set
// RepositionController when the view is attached to a hierarchy with a widget
// to make sure the view has a parent to change the position.
class RepositionController {
 public:
  explicit RepositionController(views::View* host_view, int parent_padding = 0);
  RepositionController(const RepositionController&) = delete;
  RepositionController& operator=(const RepositionController&) = delete;
  ~RepositionController();

  void OnMousePressed(const ui::MouseEvent& event);
  void OnMouseDragged(const ui::MouseEvent& event);
  bool OnMouseReleased(const ui::MouseEvent& event);
  bool OnGestureEvent(ui::GestureEvent* event);
  bool OnKeyPressed(const ui::KeyEvent& event);
  bool OnKeyReleased(const ui::KeyEvent& event);

  void set_first_dragging_callback(base::RepeatingClosure callback) {
    first_dragging_callback_ = std::move(callback);
  }
  void set_dragging_callback(base::RepeatingClosure callback) {
    dragging_callback_ = std::move(callback);
  }
  void set_mouse_drag_end_callback(base::RepeatingClosure callback) {
    mouse_drag_end_callback_ = std::move(callback);
  }
  void set_gesture_drag_end_callback(base::RepeatingClosure callback) {
    gesture_drag_end_callback_ = std::move(callback);
  }
  void set_key_pressed_callback(base::RepeatingClosure callback) {
    key_pressed_callback_ = std::move(callback);
  }
  void set_key_released_callback(base::RepeatingClosure callback) {
    key_released_callback_ = std::move(callback);
  }

 private:
  void OnDragStart(const ui::LocatedEvent& event);
  void OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd(const ui::LocatedEvent& event);

  raw_ptr<views::View> host_view_ = nullptr;
  int parent_padding_ = 0;

  base::RepeatingClosure first_dragging_callback_;
  base::RepeatingClosure dragging_callback_;
  base::RepeatingClosure mouse_drag_end_callback_;
  base::RepeatingClosure gesture_drag_end_callback_;
  base::RepeatingClosure key_pressed_callback_;
  base::RepeatingClosure key_released_callback_;

  // If `host_view` is in the dragging state.
  bool is_dragging_ = false;
  // LocatedEvent's position when drag starts.
  gfx::Point start_drag_event_pos_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_REPOSITION_CONTROLLER_H_
