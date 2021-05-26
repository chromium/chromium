// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_

#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_export.h"
#include "ui/events/types/event_type.h"

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

  // AppsGridView:
  gfx::Insets GetTilePadding() const override;
  gfx::Size GetTileGridSize() const override;

 private:
  // Created by AppListMainView, owned by views hierarchy.
  ContentsView* const contents_view_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
