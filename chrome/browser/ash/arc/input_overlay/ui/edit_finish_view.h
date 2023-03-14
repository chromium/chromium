// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_FINISH_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_FINISH_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"

namespace ui {
class Cursor;
}  // namespace ui

namespace arc::input_overlay {

class DisplayOverlayController;

// EditFinishView displays the 3 possible options to finish edit mode.
//
// These actions refer to what the user can do wrt customized key-bindings, they
// can either reset to a set of default key-bindings or just accept/cancel the
// ongoing changes.
//
// View looks like this:
// +----------------------+
// |   Reset to defaults  |
// |                      |
// |         Save         |
// |                      |
// |        Cancel        |
// +----------------------+
class EditFinishView : public views::View {
 public:
  static EditFinishView* BuildView(
      DisplayOverlayController* display_overlay_controller,
      views::View* parent);

  explicit EditFinishView(DisplayOverlayController* display_overlay_controller);

  EditFinishView(const EditFinishView&) = delete;
  EditFinishView& operator=(const EditFinishView&) = delete;
  ~EditFinishView() override;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;

 private:
  class ChildButton;

  void Init(const gfx::Size& parent_size);
  int CalculateWidth();
  void OnResetButtonPressed();
  void OnSaveButtonPressed();
  void OnCancelButtonPressed();

  // Focus ring specs.
  void SetFocusRing();

  // Drag operations.
  void OnDragStart(const ui::LocatedEvent& event);
  void OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd();

  void SetCursor(ui::mojom::CursorType cursor_type);

  raw_ptr<ChildButton> reset_button_ = nullptr;
  raw_ptr<ChildButton> save_button_ = nullptr;
  raw_ptr<ChildButton> cancel_button_ = nullptr;

  // DisplayOverlayController owns |this| class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;

  // LocatedEvent's position when drag starts.
  gfx::Point start_drag_event_pos_;
  // This view's position when drag starts.
  gfx::Point start_drag_view_pos_;
  // If this view is in a dragging state.
  bool is_dragging_ = false;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_FINISH_VIEW_H_
