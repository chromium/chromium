// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_controller.h"

#include "ash/shell.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

DeskBarController::DeskBarController() {
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

DeskBarController::~DeskBarController() {
  DestroyAllDeskBars();
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void DeskBarController::OnOverviewModeWillStart() {
  DestroyAllDeskBars();
}

void DeskBarController::OnTabletModeStarting() {
  DestroyAllDeskBars();
}

DeskBarViewBase* DeskBarController::GetDeskBarView(aura::Window* root) const {
  auto it = base::ranges::find(desk_bar_views_, root, &DeskBarViewBase::root);
  return it != desk_bar_views_.end() ? *it : nullptr;
}

void DeskBarController::CreateDeskBar(aura::Window* root) {
  // Destroys existing bar for `root` before creating a new one.
  if (GetDeskBarView(root)) {
    DestroyDeskBar(root);
  }
  CHECK(!GetDeskBarView(root));

  // Calculates bounds and creates a new bar.
  gfx::Rect bounds = GetDeskBarWidgetBounds(root);
  std::unique_ptr<views::Widget> desk_bar_widget =
      DeskBarViewBase::CreateDeskWidget(root, bounds,
                                        DeskBarViewBase::Type::kDeskButton);
  // TODO(yongshun): Replace base class with derived class once available.
  DeskBarViewBase* desk_bar_view =
      desk_bar_widget->SetContentsView(std::make_unique<DeskBarViewBase>(
          root, DeskBarViewBase::Type::kDeskButton));
  desk_bar_view->Init();

  // Ownership transfer and bookkeeping.
  desk_bar_widgets_.push_back(std::move(desk_bar_widget));
  desk_bar_views_.push_back(desk_bar_view);
}

void DeskBarController::DestroyDeskBar(aura::Window* root) {
  auto bar_view_it =
      base::ranges::find(desk_bar_views_, root, &DeskBarViewBase::root);
  CHECK(bar_view_it != desk_bar_views_.end());

  auto bar_widget_it =
      base::ranges::find(desk_bar_widgets_, (*bar_view_it)->GetWidget(),
                         &std::unique_ptr<views::Widget>::get);
  CHECK(bar_widget_it != desk_bar_widgets_.end());

  desk_bar_views_.erase(bar_view_it);
  desk_bar_widgets_.erase(bar_widget_it);
}

void DeskBarController::DestroyAllDeskBars() {
  desk_bar_views_.clear();
  desk_bar_widgets_.clear();
}

void DeskBarController::ShowDeskBar(aura::Window* root) {
  DeskBarViewBase* bar_view = GetDeskBarView(root);
  CHECK(bar_view);

  views::Widget* bar_widget = bar_view->GetWidget();
  CHECK(bar_widget);

  bar_widget->Show();
}

void DeskBarController::HideDeskBar(aura::Window* root) {
  DeskBarViewBase* bar_view = GetDeskBarView(root);
  CHECK(bar_view);

  views::Widget* bar_widget = bar_view->GetWidget();
  CHECK(bar_widget);

  bar_widget->Hide();
}

gfx::Rect DeskBarController::GetDeskBarWidgetBounds(aura::Window* root) const {
  // TODO(yongshun): Calculate bar widget bounds given shelf settings in `root`.
  return gfx::Rect(0, 0, 0, 0);
}

}  // namespace ash
