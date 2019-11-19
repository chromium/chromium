// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace ui {
class GestureEvent;
class MouseEvent;
}

namespace ash {

class WindowState;
class WorkspaceEventHandlerTestHelper;

// Handles events on workspace windows, such as double-click on the resize edge
// to maximize in one dimension.
class ASH_EXPORT WorkspaceEventHandler : public ui::EventHandler {
 public:
  explicit WorkspaceEventHandler(aura::Window* workspace_window);
  ~WorkspaceEventHandler() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  friend class WorkspaceEventHandlerTestHelper;

  // Determines if |event| corresponds to a double click on either the top or
  // bottom vertical resize edge, and if so toggles the vertical height of the
  // window between its restored state and the full available height of the
  // workspace.
  void HandleVerticalResizeDoubleClick(WindowState* window_state,
                                       ui::MouseEvent* event);

  aura::Window* workspace_window_;

  MultiWindowResizeController multi_window_resize_controller_;

  // The non-client component for the target of a MouseEvent or GestureEvent.
  // Events can be destructive to the window tree, which can cause the
  // component of a ui::EF_IS_DOUBLE_CLICK event to no longer be the same as
  // that of the initial click. Acting on a double click should only occur for
  // matching components. This will be set for left clicks, and tap events.
  int click_component_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandler);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
