// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_controller.h"
#include <algorithm>
#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_bar_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

DeskBarController::DeskBarController() {
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  DesksController::Get()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->AddPreTargetHandler(this);
}

DeskBarController::~DeskBarController() {
  CloseAllDeskBars();
  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  DesksController::Get()->RemoveObserver(this);
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void DeskBarController::OnDeskSwitchAnimationLaunching() {
  CloseAllDeskBars();
}

void DeskBarController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    OnMaybePressOffBar(*event);
  }
}

void DeskBarController::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED) {
    OnMaybePressOffBar(*event);
  }
}

void DeskBarController::OnOverviewModeWillStart() {
  CloseAllDeskBars();
}

void DeskBarController::OnTabletModeStarting() {
  CloseAllDeskBars();
}

void DeskBarController::OnWindowActivated(ActivationReason reason,
                                          aura::Window* gained_active,
                                          aura::Window* lost_active) {
  // Closing the bar for "press" type events is handled by
  // `ui::EventHandler`. Activation can change when a user merely moves the
  // cursor outside the bar when `FocusFollowsCursor` is enabled, so losing
  // activation should *not* close the bar.
  if (reason == wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT) {
    return;
  }

  // Destroys the bar when it loses activation, or any other window gains
  // activation.
  for (const auto& bar_widget : desk_bar_widgets_) {
    CHECK(bar_widget);
    CHECK(bar_widget->GetNativeWindow());
    if ((lost_active && bar_widget->GetNativeWindow()->Contains(lost_active)) ||
        (gained_active &&
         !bar_widget->GetNativeWindow()->Contains(gained_active))) {
      CloseAllDeskBars();
      DestroyAllDeskBars();
    }
  }
}

DeskBarViewBase* DeskBarController::GetDeskBarView(aura::Window* root) const {
  auto it = base::ranges::find(desk_bar_views_, root, &DeskBarViewBase::root);
  return it != desk_bar_views_.end() ? *it : nullptr;
}

void DeskBarController::OpenDeskBar(aura::Window* root) {
  CHECK(root && root->IsRootWindow());

  DeskBarViewBase* bar_view = GetDeskBarView(root);
  if (!bar_view) {
    CreateDeskBar(root);
    bar_view = GetDeskBarView(root);
  }
  CHECK(bar_view);

  views::Widget* bar_widget = bar_view->GetWidget();
  CHECK(bar_widget);

  SetDeskButtonActivation(root, /*is_activated=*/true);
  bar_widget->Show();
}

void DeskBarController::CloseDeskBar(aura::Window* root) {
  CHECK(root && root->IsRootWindow());

  DeskBarViewBase* bar_view = GetDeskBarView(root);
  CHECK(bar_view);

  views::Widget* bar_widget = bar_view->GetWidget();
  CHECK(bar_widget);

  SetDeskButtonActivation(root, /*is_activated=*/false);
  bar_widget->Hide();
}

void DeskBarController::CloseAllDeskBars() {
  for (auto* bar_view : desk_bar_views_) {
    views::Widget* bar_widget = bar_view->GetWidget();
    CHECK(bar_widget);

    if (bar_widget->IsVisible()) {
      SetDeskButtonActivation(bar_view->root(), /*is_activated=*/false);
      bar_widget->Hide();
    }
  }
}

void DeskBarController::CreateDeskBar(aura::Window* root) {
  CHECK_EQ(desk_bar_views_.size(), desk_bar_widgets_.size());

  // Closes existing bar for `root` before creating a new one.
  if (GetDeskBarView(root)) {
    CloseDeskBar(root);
  }
  CHECK(!GetDeskBarView(root));

  // Calculates bounds and creates a new bar.
  gfx::Rect bounds = GetDeskBarWidgetBounds(root);
  std::unique_ptr<views::Widget> desk_bar_widget =
      DeskBarViewBase::CreateDeskWidget(root, bounds,
                                        DeskBarViewBase::Type::kDeskButton);
  DeskBarView* desk_bar_view =
      desk_bar_widget->SetContentsView(std::make_unique<DeskBarView>(root));
  desk_bar_view->Init();

  // Ownership transfer and bookkeeping.
  desk_bar_views_.push_back(desk_bar_view);
  desk_bar_widgets_.push_back(std::move(desk_bar_widget));
}

void DeskBarController::DestroyAllDeskBars() {
  CHECK_EQ(desk_bar_views_.size(), desk_bar_widgets_.size());

  desk_bar_views_.clear();

  // Deletes asynchronously so it is less likely to result in UAF.
  for (auto& bar_widget : desk_bar_widgets_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, bar_widget.release());
  }
  desk_bar_widgets_.clear();
}

gfx::Rect DeskBarController::GetDeskBarWidgetBounds(aura::Window* root) const {
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(root)->user_work_area_bounds();
  gfx::Size bar_size(work_area.width(),
                     DeskBarViewBase::GetPreferredBarHeight(
                         root, DeskBarViewBase::Type::kDeskButton,
                         DeskBarViewBase::State::kExpanded));

  const Shelf* shelf = Shelf::ForWindow(root);
  gfx::Rect shelf_bounds = shelf->GetShelfBoundsInScreen();
  gfx::Rect desk_button_bounds =
      shelf->desk_button_widget()->GetWindowBoundsInScreen();

  gfx::Point bar_origin;
  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
      bar_origin.set_x(shelf_bounds.x() +
                       (work_area.width() - bar_size.width()) / 2);
      bar_origin.set_y(shelf_bounds.y() - kDeskBarShelfAndBarSpacing -
                       bar_size.height());
      break;
    case ShelfAlignment::kLeft:
      bar_size.set_width(bar_size.width() - kDeskBarShelfAndBarSpacing);
      bar_origin.set_x(shelf_bounds.right() + kDeskBarShelfAndBarSpacing);
      bar_origin.set_y(desk_button_bounds.y());
      break;
    case ShelfAlignment::kRight:
      bar_size.set_width(bar_size.width() - kDeskBarShelfAndBarSpacing);
      bar_origin.set_x(shelf_bounds.x() - kDeskBarShelfAndBarSpacing -
                       bar_size.width());
      bar_origin.set_y(desk_button_bounds.y());
      break;
    default:
      NOTREACHED_NORETURN();
  }

  return {bar_origin, bar_size};
}

void DeskBarController::OnMaybePressOffBar(const ui::LocatedEvent& event) {
  if (desk_bar_views_.empty()) {
    return;
  }

  // Does nothing for the press within the bar since it is handled by the bar
  // view. Otherwise, we should either commit the desk name changes or close the
  // bars.
  bool intersect_with_bar_view = false;
  bool intersect_with_desk_button = false;
  bool desk_name_being_modified = false;
  for (auto* desk_bar_view : desk_bar_views_) {
    // Converts to screen coordinate.
    gfx::Point screen_location;
    gfx::Rect desk_bar_view_bounds = desk_bar_view->GetBoundsInScreen();
    gfx::Rect desk_button_bounds =
        GetDeskButton(desk_bar_view->root())->GetBoundsInScreen();
    if (event.target()) {
      screen_location = event.target()->GetScreenLocation(event);
    } else {
      screen_location = event.root_location();
      wm::ConvertPointToScreen(desk_bar_view->root(), &screen_location);
    }

    if (desk_bar_view_bounds.Contains(screen_location)) {
      intersect_with_bar_view = true;
    } else if (desk_bar_view->IsDeskNameBeingModified()) {
      desk_name_being_modified = true;
      DeskNameView::CommitChanges(desk_bar_view->GetWidget());
    }

    if (desk_button_bounds.Contains(screen_location)) {
      intersect_with_desk_button = true;
    }
  }

  if (!intersect_with_bar_view && !desk_name_being_modified &&
      !intersect_with_desk_button) {
    CloseAllDeskBars();
  }
}

DeskButton* DeskBarController::GetDeskButton(aura::Window* root) {
  return Shelf::ForWindow(root)->desk_button_widget()->GetDeskButton();
}

void DeskBarController::SetDeskButtonActivation(aura::Window* root,
                                                bool is_activated) {
  GetDeskButton(root)->SetActivation(/*is_activated=*/is_activated);
}

}  // namespace ash
