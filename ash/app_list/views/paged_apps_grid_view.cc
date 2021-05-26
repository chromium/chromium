// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/paged_apps_grid_view.h"

#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/check.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

PagedAppsGridView::PagedAppsGridView(
    ContentsView* contents_view,
    AppsGridViewFolderDelegate* folder_delegate)
    : AppsGridView(contents_view,
                   contents_view->GetAppListMainView()->view_delegate(),
                   folder_delegate),
      contents_view_(contents_view) {
  DCHECK(contents_view_);
}

PagedAppsGridView::~PagedAppsGridView() = default;

void PagedAppsGridView::HandleScrollFromAppListView(const gfx::Vector2d& offset,
                                                    ui::EventType type) {
  // If |pagination_model_| is empty, don't handle scroll events.
  if (pagination_model_.total_pages() <= 0)
    return;

  // Maybe switch pages.
  pagination_controller_->OnScroll(offset, type);
}

gfx::Insets PagedAppsGridView::GetTilePadding() const {
  if (is_in_folder()) {
    const int tile_padding_in_folder =
        GetAppListConfig().grid_tile_spacing_in_folder() / 2;
    return gfx::Insets(-tile_padding_in_folder, -tile_padding_in_folder);
  }
  return gfx::Insets(-vertical_tile_padding(), -horizontal_tile_padding());
}

gfx::Size PagedAppsGridView::GetTileGridSize() const {
  gfx::Rect rect(GetTotalTileSize());
  rect.set_size(
      gfx::Size(rect.width() * cols(), rect.height() * rows_per_page()));
  rect.Inset(-GetTilePadding());
  return rect.size();
}

}  // namespace ash
