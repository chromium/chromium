// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_FOLDER_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_FOLDER_DELEGATE_H_

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/apps_grid_view.h"

namespace gfx {
class Point;
}

namespace ash {

class AppListItemView;

// A delegate which allows an AppsGridView to communicate with its host folder.
class APP_LIST_EXPORT AppsGridViewFolderDelegate {
 public:
  // Called when a folder item is dragged out of the folder to be re-parented.
  // |original_drag_view| is the |drag_view_| inside the folder's grid view.
  // |drag_point_in_folder_grid| is the last drag point in coordinate of the
  // AppsGridView inside the folder.
  virtual void ReparentItem(AppListItemView* original_drag_view,
                            const gfx::Point& drag_point_in_folder_grid,
                            bool has_native_drag) = 0;

  // Dispatches drag event from the hidden grid view to the root level grid view
  // for re-parenting a folder item.
  virtual void DispatchDragEventForReparent(
      AppsGridView::Pointer pointer,
      const gfx::Point& drag_point_in_folder_grid) = 0;

  // Dispatches EndDrag event from the hidden grid view to the root level grid
  // view for reparenting a folder item.
  // |events_forwarded_to_drag_drop_host|: True if the dragged item is dropped
  // to the drag_drop_host, eg. dropped on shelf.
  // |cancel_drag|: True if the drag is ending because it has been canceled.
  virtual void DispatchEndDragEventForReparent(
      bool events_forwarded_to_drag_drop_host,
      bool cancel_drag) = 0;

  // Returns true if |point| falls outside of the folder container ink bubble.
  virtual bool IsPointOutsideOfFolderBoundary(const gfx::Point& point) = 0;

  // Returns true if the associated folder item is an OEM folder.
  virtual bool IsOEMFolder() const = 0;

  // Hides or show the root level's grid view. This is needed so that the
  // synchronous drag has an icon for reparenting while it loads.
  virtual void SetRootLevelDragViewVisible(bool visible) = 0;

  // Moves |reparented_item| to the root level's grid view, left/right/up/down
  // of the folder's grid position.
  virtual void HandleKeyboardReparent(AppListItemView* reparented_view,
                                      ui::KeyboardCode key_code) = 0;

 protected:
  virtual ~AppsGridViewFolderDelegate() {}
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_FOLDER_DELEGATE_H_
