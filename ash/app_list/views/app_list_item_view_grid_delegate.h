// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_GRID_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_GRID_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class Event;
class LocatedEvent;
}  // namespace ui

namespace ash {
class AppListItemView;

// The parent apps grid (AppsGridView) or a stub. Not named "Delegate" to
// differentiate it from AppListViewDelegate.
class ASH_EXPORT AppListItemViewGridDelegate {
 public:
  virtual ~AppListItemViewGridDelegate() = default;

  // Whether the parent apps grid (if any) is a folder.
  virtual bool IsInFolder() const = 0;

  // Methods for keyboard selection.
  virtual void SetSelectedView(AppListItemView* view) = 0;
  virtual void ClearSelectedView() = 0;
  virtual bool IsSelectedView(const AppListItemView* view) const = 0;

  // Registers `view` as a dragged item with the apps grid. Called when the
  // user presses the mouse, or starts touch interaction with the view (both
  // of which may transition into a drag operation).
  // `location` - The pointer location in the view's bounds.
  // `root_location` - The pointer location in the root window coordinates.
  // `drag_start_callback` - Callback that gets called when the mouse/touch
  //     interaction transitions into a drag (i.e. when the "drag" item starts
  //     moving.
  //  `drag_end_callback` - Callback that gets called when drag interaction
  //     ends.
  //  Returns whether `view` has been registered as a dragged view. Callbacks
  //  should be ignored if the method returns false. If the method returns
  //  true, it's expected to eventually run `drag_end_callback`.
  virtual bool InitiateDrag(AppListItemView* view,
                            const gfx::Point& location,
                            const gfx::Point& root_location,
                            base::OnceClosure drag_start_callback,
                            base::OnceClosure drag_end_callback) = 0;
  virtual void StartDragAndDropHostDragAfterLongPress() = 0;
  // Called from AppListItemView when it receives a drag event. Returns true
  // if the drag is still happening.
  virtual bool UpdateDragFromItem(bool is_touch,
                                  const ui::LocatedEvent& event) = 0;
  virtual void EndDrag(bool cancel) = 0;

  // Provided as a callback for AppListItemView to notify of activation via
  // press/click/return key.
  virtual void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                          const ui::Event& event) = 0;

  // Whether the app list item view would be visible in the default apps grid
  // view state. For example, for scrollable apps grid view, an item view would
  // be above the fold if it were visible when the apps grid view is not
  // scrolled.
  virtual bool IsAboveTheFold(AppListItemView* item_view) = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_GRID_DELEGATE_H_
