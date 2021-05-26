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
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
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

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler:

void PagedAppsGridView::OnGestureEvent(ui::GestureEvent* event) {
  // If a tap/long-press occurs within a valid tile, it is usually a mistake and
  // should not close the launcher in clamshell mode. Otherwise, we should let
  // those events pass to the ancestor views.
  if (!IsTabletMode() && (event->type() == ui::ET_GESTURE_TAP ||
                          event->type() == ui::ET_GESTURE_LONG_PRESS)) {
    if (EventIsBetweenOccupiedTiles(event)) {
      contents_view_->app_list_view()->CloseKeyboardIfVisible();
      event->SetHandled();
    }
    return;
  }

  if (!ShouldHandleDragEvent(*event))
    return;

  // Scroll begin events should not be passed to ancestor views from apps grid
  // in our current design. This prevents both ignoring horizontal scrolls in
  // app list, and closing open folders.
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()) ||
      event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    event->SetHandled();
  }
}

void PagedAppsGridView::OnMouseEvent(ui::MouseEvent* event) {
  if (IsTabletMode() || !event->IsLeftMouseButton())
    return;

  gfx::PointF point_in_root = event->root_location_f();

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      if (!EventIsBetweenOccupiedTiles(event))
        break;
      event->SetHandled();
      mouse_drag_start_point_ = point_in_root;
      last_mouse_drag_point_ = point_in_root;
      // Manually send the press event to the AppListView to update drag root
      // location
      contents_view_->app_list_view()->OnMouseEvent(event);
      break;
    case ui::ET_MOUSE_DRAGGED:
      if (!ShouldHandleDragEvent(*event)) {
        // We need to send mouse drag/release events to AppListView explicitly
        // because AppsGridView handles the mouse press event and gets captured.
        // Then AppListView cannot receive mouse drag/release events implcitly.

        // Send the fabricated mouse press event to AppListView if AppsGridView
        // is not in mouse drag yet.
        gfx::Point drag_location_in_app_list;
        if (!is_in_mouse_drag_) {
          ui::MouseEvent press_event(
              *event, static_cast<views::View*>(this),
              static_cast<views::View*>(contents_view_->app_list_view()),
              ui::ET_MOUSE_PRESSED, event->flags());
          contents_view_->app_list_view()->OnMouseEvent(&press_event);

          is_in_mouse_drag_ = true;
        }

        drag_location_in_app_list = event->location();
        ConvertPointToTarget(this, contents_view_->app_list_view(),
                             &drag_location_in_app_list);
        event->set_location(drag_location_in_app_list);
        contents_view_->app_list_view()->OnMouseEvent(event);
        break;
      }
      event->SetHandled();
      if (!is_in_mouse_drag_) {
        if (abs(point_in_root.y() - mouse_drag_start_point_.y()) <
            kMouseDragThreshold) {
          break;
        }
        pagination_controller_->StartMouseDrag(point_in_root -
                                               mouse_drag_start_point_);
        is_in_mouse_drag_ = true;
      }

      if (!is_in_mouse_drag_)
        break;

      pagination_controller_->UpdateMouseDrag(
          point_in_root - last_mouse_drag_point_, GetContentsBounds());
      last_mouse_drag_point_ = point_in_root;
      break;
    case ui::ET_MOUSE_RELEASED: {
      // Calculate |should_handle| before resetting |mouse_drag_start_point_|
      // because ShouldHandleDragEvent depends on its value.
      const bool should_handle = ShouldHandleDragEvent(*event);

      is_in_mouse_drag_ = false;
      mouse_drag_start_point_ = gfx::PointF();
      last_mouse_drag_point_ = gfx::PointF();

      if (!should_handle) {
        gfx::Point drag_location_in_app_list = event->location();
        ConvertPointToTarget(this, contents_view_->app_list_view(),
                             &drag_location_in_app_list);
        event->set_location(drag_location_in_app_list);
        contents_view_->app_list_view()->OnMouseEvent(event);
        break;
      }
      event->SetHandled();
      pagination_controller_->EndMouseDrag(*event);
      break;
    }
    default:
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppsGridView:

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

////////////////////////////////////////////////////////////////////////////////
// private:

bool PagedAppsGridView::ShouldHandleDragEvent(const ui::LocatedEvent& event) {
  // If |pagination_model_| is empty, don't handle scroll events.
  if (pagination_model_.total_pages() <= 0)
    return false;

  DCHECK(event.IsGestureEvent() || event.IsMouseEvent());

  // If the event is a scroll down in clamshell mode on the first page, don't
  // let |pagination_controller_| handle it. Unless it occurs in a folder.
  auto calculate_offset = [this](const ui::LocatedEvent& event) -> int {
    if (event.IsGestureEvent())
      return event.AsGestureEvent()->details().scroll_y_hint();
    gfx::PointF root_location = event.root_location_f();
    return root_location.y() - mouse_drag_start_point_.y();
  };
  if (!is_in_folder() &&
      (event.IsMouseEvent() || event.type() == ui::ET_GESTURE_SCROLL_BEGIN) &&
      !IsTabletMode() &&
      ((pagination_model_.selected_page() == 0 &&
        calculate_offset(event) > 0) ||
       contents_view_->app_list_view()->is_in_drag())) {
    return false;
  }

  return true;
}

}  // namespace ash
