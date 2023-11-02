// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_

#include <memory>
#include <string>

#include "ash/app_list/views/app_drag_icon_proxy.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

class AppDragIconProxy;

// This class will get used by the AppListView to drag and drop Application
// shortcuts onto another host (the shelf).
class ApplicationDragAndDropHost {
 public:
  // Returns whether the host wants to handle the drag operation.
  virtual bool ShouldHandleDrag(const std::string& app_id,
                                const gfx::Point& location_in_screen) const = 0;

  // A drag operation could get started. The recipient has to return true if
  // it wants to take it - e.g. `location_in_screen` is over a
  // target area. The passed `app_id` identifies the application which should
  // get dropped.
  virtual bool StartDrag(const std::string& app_id,
                         const gfx::Point& location_in_screen,
                         const gfx::Rect& drag_icon_bounds_in_screen) = 0;

  // This gets only called when the `StartDrag()` function returned true and it
  // dispatches the mouse coordinate change accordingly. When the function
  // returns false it requests that the operation be aborted since the event
  // location is out of bounds.
  virtual bool Drag(const gfx::Point& location_in_screen,
                    const gfx::Rect& drag_icon_bounds_in_screen) = 0;

  // Once `StartDrag()` returned true, this function is guaranteed to be called
  // when the mouse / touch events stop. If `cancel` is set, the drag operation
  // was aborted, otherwise the change should be kept.
  // `icon_proxy` is a drag icon proxy that was created for drag, which will be
  // set if the host is expected to handle icon animations on drag end.
  virtual void EndDrag(bool cancel,
                       std::unique_ptr<AppDragIconProxy> icon_proxy) = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_
