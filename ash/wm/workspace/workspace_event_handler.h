// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace ui {
class GestureEvent;
class MouseEvent;
}

namespace ash {

class MultiWindowResizeController;
class WindowState;
class WorkspaceEventHandlerTestHelper;

// Handles events on workspace windows, such as double-click on the resize edge
// to maximize in one dimension.
class ASH_EXPORT WorkspaceEventHandler : public ui::EventHandler {
 public:
  explicit WorkspaceEventHandler(aura::Window* workspace_window);

  WorkspaceEventHandler(const WorkspaceEventHandler&) = delete;
  WorkspaceEventHandler& operator=(const WorkspaceEventHandler&) = delete;

  ~WorkspaceEventHandler() override;

  MultiWindowResizeController* multi_window_resize_controller() const {
    return multi_window_resize_controller_.get();
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  friend class WorkspaceEventHandlerTestHelper;

  // Determines if `event` corresponds to a double click on a resize edge, and
  // if so toggles the width/height of the window (width when the left or right
  // edge is double clicked, height when the top or bottom edge is double
  // clicked) between its restored state and the full available width/height of
  // the workspace.
  void HandleResizeDoubleClick(WindowState* window_state,
                               ui::MouseEvent* event);

  raw_ptr<aura::Window> workspace_window_;

  // Handles moving two windows that are side by side together at once. Not
  // created for the float container.
  std::unique_ptr<MultiWindowResizeController> multi_window_resize_controller_;

  // The non-client component for the target of a MouseEvent or GestureEvent.
  // Events can be destructive to the window tree, which can cause the
  // component of a ui::EF_IS_DOUBLE_CLICK event to no longer be the same as
  // that of the initial click. Acting on a double click should only occur for
  // matching components. This will be set for left clicks, and tap events.
  int click_component_;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
