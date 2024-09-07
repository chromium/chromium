// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_PANEL_VIEW_H_
#define ASH_WM_SYSTEM_PANEL_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace ash {

// `SystemPanelView` is a movable panel that can be dragged from any point
// within the view itself. Other drag behavior, e.g. for text selection, may be
// handled by the panel's child views.
class ASH_EXPORT SystemPanelView : public views::View {
  METADATA_HEADER(SystemPanelView, views::View)

 public:
  // Handles logic for dragging to reposition the `SystemPanelView`.
  class DragController {
   public:
    DragController() = default;
    DragController(const DragController&) = delete;
    DragController& operator=(const DragController&) = delete;
    ~DragController() = default;

    // Handles mouse or gesture drag events to reposition `widget`. Events
    // that are not part of a drag event sequence are ignored.
    void OnLocatedPanelEvent(views::Widget* const widget,
                             ui::LocatedEvent* event);

   private:
    // Whether a drag is currently in progress.
    bool is_dragging_ = false;

    // The start coordinates of the most recent press or drag begin event
    // handled by the controller, in screen coordinates.
    gfx::Point start_dragging_event_location_;

    // The initial bounds of the panel of the most recent press or drag begin
    // event handled by the controller, in screen coordinates.
    gfx::Rect panel_widget_initial_bounds_;
  };

  SystemPanelView();
  SystemPanelView(const SystemPanelView&) = delete;
  SystemPanelView& operator=(const SystemPanelView&) = delete;
  ~SystemPanelView() override;

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Handles drag events to reposition the panel. Events that are not part of a
  // drag event sequence are ignored.
  void HandleDragEventIfNeeded(ui::LocatedEvent* event);

  DragController drag_controller_;
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_PANEL_VIEW_H_
