// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_

#include <string>

namespace gfx {
class ImageSkia;
class Point;
class Vector2d;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace ash {

// This class will get used by the AppListView to drag and drop Application
// shortcuts onto another host (the launcher).
class ApplicationDragAndDropHost {
 public:
  // Creates an OS dependent drag proxy icon which can escape the given view.
  // The proxy should get created using the |icon| with a magnification of
  // |scale_factor| at a center location of |location_in_screen_coordinates.
  // Use |replaced_view| to find the screen which is used.
  // The |cursor_offset_from_center| is the offset from the mouse cursor to
  // the center of the item.
  virtual void CreateDragIconProxy(
      const gfx::Point& location_in_screen_coordinates,
      const gfx::ImageSkia& icon,
      views::View* replaced_view,
      const gfx::Vector2d& cursor_offset_from_center,
      float scale_factor) {}

  // Creates an OS dependent drag proxy icon which can escape the given view.
  // The proxy should get created using the |icon| with a magnification of
  // |scale_factor| with its origin at |origin_in_screen_coordinates|.
  // Use |replaced_view| to find the screen which is used.
  // The proxy will be created without any visibility animation effect.
  // Sets background blur with specified blur radius to the dragged icon if
  // |blur_radius| is larger than 0.
  virtual void CreateDragIconProxyByLocationWithNoAnimation(
      const gfx::Point& origin_in_screen_coordinates,
      const gfx::ImageSkia& icon,
      views::View* replaced_view,
      float scale_factor,
      int blur_radius) = 0;

  // Updates the screen location of the Drag icon proxy.
  virtual void UpdateDragIconProxy(
      const gfx::Point& location_in_screen_coordinates) {}

  // Updates the screen location of the Drag icon proxy with its origin at
  // |origin_in_screen_coordinates|.
  virtual void UpdateDragIconProxyByLocation(
      const gfx::Point& origin_in_screen_coordinates) {}

  // Removes the OS dependent drag proxy from the screen.
  virtual void DestroyDragIconProxy() = 0;

  // A drag operation could get started. The recipient has to return true if
  // it wants to take it - e.g. |location_in_screen_poordinates| is over a
  // target area. The passed |app_id| identifies the application which should
  // get dropped.
  virtual bool StartDrag(const std::string& app_id,
                         const gfx::Point& location_in_screen_coordinates) = 0;

  // This gets only called when the |StartDrag| function returned true and it
  // dispatches the mouse coordinate change accordingly. When the function
  // returns false it requests that the operation be aborted since the event
  // location is out of bounds.
  // Note that this function does not update the drag proxy's screen position.
  virtual bool Drag(const gfx::Point& location_in_screen_coordinates) = 0;

  // Once |StartDrag| returned true, this function is guaranteed to be called
  // when the mouse / touch events stop. If |cancel| is set, the drag operation
  // was aborted, otherwise the change should be kept.
  virtual void EndDrag(bool cancel) {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_DRAG_AND_DROP_HOST_H_
