// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_DRAG_CONTROLLER_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_DRAG_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ref.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class LocatedEvent;
}

namespace ash {

class MahiUiController;

// Handles logic for dragging to reposition the Mahi panel.
class ASH_EXPORT MahiPanelDragController {
 public:
  explicit MahiPanelDragController(MahiUiController* panel_view);
  MahiPanelDragController(const MahiPanelDragController&) = delete;
  MahiPanelDragController& operator=(const MahiPanelDragController&) = delete;
  ~MahiPanelDragController();

  // Handles mouse or gesture drag events to reposition the Mahi panel. Events
  // that are not part of a drag event sequence are ignored.
  void OnLocatedPanelEvent(ui::LocatedEvent* event);

 private:
  // Whether a drag is currently in progress.
  bool is_dragging_ = false;

  // The start coordinates of the most recent press or drag begin event handled
  // by the controller, in screen coordinates.
  gfx::Point start_dragging_event_location_;

  // The Mahi UI controller which owns this.
  const raw_ref<MahiUiController> ui_controller_;

  // The initial bounds of the panel of the most recent press or drag begin
  // event handled by the controller, in screen coordinates.
  gfx::Rect panel_widget_initial_bounds_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_DRAG_CONTROLLER_H_
