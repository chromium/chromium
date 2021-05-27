// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_

#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_export.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class Vector2d;
}  // namespace gfx

namespace ash {

class ContentsView;

// An apps grid that shows the apps on a series of fixed-size pages.
// Used for the peeking/fullscreen launcher, home launcher and folders.
// Created by and is a child of AppsContainerView.
class ASH_EXPORT PagedAppsGridView : public AppsGridView {
 public:
  PagedAppsGridView(ContentsView* contents_view,
                    AppsGridViewFolderDelegate* folder_delegate);
  PagedAppsGridView(const PagedAppsGridView&) = delete;
  PagedAppsGridView& operator=(const PagedAppsGridView&) = delete;
  ~PagedAppsGridView() override;

  // Passes scroll information from AppListView to the PaginationController,
  // which may switch pages.
  void HandleScrollFromAppListView(const gfx::Vector2d& offset,
                                   ui::EventType type);

  // Updates the opacity of all the items in the grid when the grid itself is
  // being dragged. The app icons fade out as the launcher slides off the bottom
  // of the screen.
  void UpdateOpacity(bool restore_opacity);

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // AppsGridView:
  gfx::Insets GetTilePadding() const override;
  gfx::Size GetTileGridSize() const override;

 private:
  // Indicates whether the drag event (from the gesture or mouse) should be
  // handled by PagedAppsGridView.
  bool ShouldHandleDragEvent(const ui::LocatedEvent& event);

  // Created by AppListMainView, owned by views hierarchy.
  ContentsView* const contents_view_;

  // Whether the grid is in mouse drag. Used for between-item drags that move
  // the entire grid, not for app icon drags.
  bool is_in_mouse_drag_ = false;

  // The initial mouse drag location in root window coordinate. Updates when the
  // drag on PagedAppsGridView starts. Used for between-item drags that move the
  // entire grid, not for app icon drags.
  gfx::PointF mouse_drag_start_point_;

  // The last mouse drag location in root window coordinate. Used for
  // between-item drags that move the entire grid, not for app icon drags.
  gfx::PointF last_mouse_drag_point_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
