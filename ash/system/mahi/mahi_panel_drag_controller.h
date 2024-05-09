// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_DRAG_CONTROLLER_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_DRAG_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ref.h"
#include "ui/gfx/geometry/point.h"

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

  // Coordinates of the most recent press or drag event handled by the
  // controller, in root window coordinates.
  gfx::Point previous_location_in_root_;

  // The Mahi UI controller which owns this.
  const raw_ref<MahiUiController> ui_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_DRAG_CONTROLLER_H_
