// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid_event_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/gestures/wm_fling_handler.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/functional/bind.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Do not bother moving the grid until a series of scrolls has reached this
// threshold.
constexpr float kScrollOffsetThresholdDp = 1.f;

WallpaperView* GetWallpaperViewForRoot(const aura::Window* root_window) {
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  if (!wallpaper_widget_controller)
    return nullptr;
  return wallpaper_widget_controller->wallpaper_view();
}

}  // namespace

OverviewGridEventHandler::OverviewGridEventHandler(OverviewGrid* grid)
    : grid_(grid), overview_session_(grid_->overview_session()) {
  DCHECK(overview_session_);
  auto* wallpaper_view = GetWallpaperViewForRoot(grid_->root_window());
  if (wallpaper_view)
    wallpaper_view->AddPreTargetHandler(this);
}

OverviewGridEventHandler::~OverviewGridEventHandler() {
  OnFlingEnd();
  grid_->EndScroll();

  auto* wallpaper_view = GetWallpaperViewForRoot(grid_->root_window());
  if (wallpaper_view)
    wallpaper_view->RemovePreTargetHandler(this);
}

void OverviewGridEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  // The following can only happen if a user is dragging a window with touch and
  // then they move the mouse to click on the wallpaper. This is an extreme edge
  // case, so just exit overview. Note that this is done here instead of on
  // release like usual, because pressing the mouse while dragging sends out a
  // ui::GESTURE_END_EVENT which may cause a bad state.
  if (event->type() == ui::EventType::kMousePressed &&
      !overview_session_->CanProcessEvent()) {
    OverviewController::Get()->EndOverview(
        OverviewEndAction::kClickingOutsideWindowsInOverview);
    event->StopPropagation();
    event->SetHandled();
    return;
  }

  if (event->type() == ui::EventType::kMouseReleased) {
    HandleClickOrTap(event);
  }
}

void OverviewGridEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (!overview_session_->CanProcessEvent()) {
    event->StopPropagation();
    event->SetHandled();
    return;
  }

  // TODO(crbug.com/1341128): Enable context menu via long-press in library page
  // `SavedDeskLibraryView` will take over gesture event if it's active. When
  // it's `EventType::kGestureTap`, here it does not set event to handled, and
  // thus `HandleClickOrTap()` would be executed from
  // `SavedDeskLibraryView::OnLocatedEvent()`.
  if (grid_->IsShowingSavedDeskLibrary()) {
    return;
  }

  if (event->type() == ui::EventType::kGestureTap) {
    HandleClickOrTap(event);
    return;
  }

  // The following events are for scrolling the overview scroll layout, which is
  // tablet only.
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  switch (event->type()) {
    case ui::EventType::kScrollFlingStart: {
      HandleFlingScroll(event);
      event->SetHandled();
      break;
    }
    case ui::EventType::kGestureScrollBegin: {
      scroll_offset_x_cumulative_ = 0.f;
      OnFlingEnd();
      grid_->StartScroll();
      event->SetHandled();
      break;
    }
    case ui::EventType::kGestureScrollUpdate: {
      // Only forward the scrolls to grid once they have exceeded the threshold.
      const float scroll_offset_x = event->details().scroll_x();
      scroll_offset_x_cumulative_ += scroll_offset_x;
      if (std::abs(scroll_offset_x_cumulative_) > kScrollOffsetThresholdDp) {
        grid_->UpdateScrollOffset(scroll_offset_x_cumulative_);
        scroll_offset_x_cumulative_ = 0.f;
      }
      event->SetHandled();
      break;
    }
    case ui::EventType::kGestureScrollEnd: {
      grid_->EndScroll();
      event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void OverviewGridEventHandler::HandleClickOrTap(ui::Event* event) {
  CHECK_EQ(ui::EP_PRETARGET, event->phase());

  // If the user is renaming a desk or saved desk, rather than closing overview
  // the focused name view should lose focus.
  if (grid_->IsDeskNameBeingModified() ||
      grid_->IsSavedDeskNameBeingModified()) {
    grid_->CommitNameChanges();
    event->StopPropagation();
    return;
  }

  if (display::Screen::GetScreen()->InTabletMode()) {
    aura::Window* window = static_cast<views::View*>(event->target())
                               ->GetWidget()
                               ->GetNativeWindow();

    // In tablet mode, clicking on tapping on the wallpaper background will
    // head back to home launcher screen if not in split view (in which case
    // the event should be ignored).
    if (!SplitViewController::Get(window)->InSplitViewMode()) {
      int64_t display_id =
          display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
      Shell::Get()->app_list_controller()->GoHome(display_id);
    }
  } else {
    OverviewController::Get()->EndOverview(
        OverviewEndAction::kClickingOutsideWindowsInOverview);
  }
  event->StopPropagation();
}

void OverviewGridEventHandler::HandleFlingScroll(ui::GestureEvent* event) {
  const gfx::Vector2dF initial_fling_velocity(event->details().velocity_x(),
                                              event->details().velocity_y());
  fling_handler_ = std::make_unique<WmFlingHandler>(
      initial_fling_velocity, grid_->root_window(),
      base::BindRepeating(&OverviewGridEventHandler::OnFlingStep,
                          base::Unretained(this)),
      base::BindRepeating(&OverviewGridEventHandler::OnFlingEnd,
                          base::Unretained(this)));
}

bool OverviewGridEventHandler::OnFlingStep(float offset) {
  // Updates `grid_` based on `offset`.
  DCHECK(fling_handler_);
  return grid_->UpdateScrollOffset(offset);
}

void OverviewGridEventHandler::OnFlingEnd() {
  if (!fling_handler_)
    return;

  fling_handler_.reset();
  grid_->EndScroll();
}

}  // namespace ash
