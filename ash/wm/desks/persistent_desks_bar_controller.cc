// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/persistent_desks_bar_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/persistent_desks_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kBarHeight = 40;

// Creates and returns the widget that contains the PersistentDesksBarView. The
// returned widget has no content view yet, and hasn't been shown yet.
std::unique_ptr<views::Widget> CreatePersistentDesksBarWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // TODO(minch): Destroy the bar and recreate it in the new primary display
  // when the current primary display is removed.
  // Create and show the bar only in the primary display for now. Since it is
  // enough to collect the metrics for the experiment, it can also avoid the bar
  // to consume space from all the displays.
  auto* root_window = Shell::GetPrimaryRootWindow();
  params.parent = root_window->GetChildById(kShellWindowId_ShelfContainer);
  gfx::Rect bounds = display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(root_window)
                         .work_area();
  bounds.set_height(kBarHeight);
  params.bounds = bounds;
  params.name = "PersistentDesksBarWidget";

  widget->Init(std::move(params));
  return widget;
}

}  // namespace

PersistentDesksBarController::PersistentDesksBarController() {
  auto* shell = Shell::Get();
  shell->session_controller()->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->desks_controller()->AddObserver(this);
  shell->tablet_mode_controller()->AddObserver(this);
  shell->AddShellObserver(this);
}

PersistentDesksBarController::~PersistentDesksBarController() {
  auto* shell = Shell::Get();
  shell->RemoveShellObserver(this);
  shell->tablet_mode_controller()->RemoveObserver(this);
  shell->desks_controller()->RemoveObserver(this);
  shell->overview_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
}

void PersistentDesksBarController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state == session_manager::SessionState::ACTIVE)
    MaybeInitBarWidget();
  else
    DestroyBarWidget();
}

void PersistentDesksBarController::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  if (!canceled)
    DestroyBarWidget();
}

void PersistentDesksBarController::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  if (!canceled)
    MaybeInitBarWidget();
}

void PersistentDesksBarController::OnDeskAdded(const Desk* desk) {
  MaybeInitBarWidget();
}

void PersistentDesksBarController::OnDeskRemoved(const Desk* desk) {
  if (!persistent_desks_bar_widget_)
    return;

  if (DesksController::Get()->desks().size() == 1)
    DestroyBarWidget();
  else
    persistent_desks_bar_view_->RefreshDeskButtons();
}

void PersistentDesksBarController::OnDeskReordered(int old_index,
                                                   int new_index) {
  // Desk reordering is supported in overview mode only. The bar should have
  // been destroyed while entering overview mode.
  DCHECK(!persistent_desks_bar_widget_);
}

void PersistentDesksBarController::OnDeskActivationChanged(
    const Desk* activated,
    const Desk* deactivated) {
  if (!persistent_desks_bar_widget_)
    return;

  persistent_desks_bar_view_->RefreshDeskButtons();
}

void PersistentDesksBarController::OnDeskSwitchAnimationLaunching() {}

void PersistentDesksBarController::OnDeskSwitchAnimationFinished() {}

void PersistentDesksBarController::OnTabletModeStarted() {
  DestroyBarWidget();
}

void PersistentDesksBarController::OnTabletModeEnded() {
  MaybeInitBarWidget();
}

void PersistentDesksBarController::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  const Shelf* shelf = Shelf::ForWindow(root_window);
  if (shelf->IsHorizontalAlignment())
    MaybeInitBarWidget();
  else
    DestroyBarWidget();
}

void PersistentDesksBarController::ToggleEnabledState() {
  is_enabled_ = !is_enabled_;
  if (!is_enabled_)
    DestroyBarWidget();
}

bool PersistentDesksBarController::ShouldPersistentDesksBarBeCreated() const {
  return is_enabled_ && !TabletMode::Get()->InTabletMode() &&
         !Shell::Get()->overview_controller()->InOverviewSession() &&
         DesksController::Get()->desks().size() > 1 &&
         Shelf::ForWindow(Shell::GetPrimaryRootWindow())
             ->IsHorizontalAlignment();
}

void PersistentDesksBarController::MaybeInitBarWidget() {
  if (!ShouldPersistentDesksBarBeCreated())
    return;

  if (!persistent_desks_bar_widget_) {
    DCHECK(!persistent_desks_bar_view_);
    persistent_desks_bar_widget_ = CreatePersistentDesksBarWidget();
    persistent_desks_bar_view_ = persistent_desks_bar_widget_->SetContentsView(
        std::make_unique<PersistentDesksBarView>());
  }
  persistent_desks_bar_view_->RefreshDeskButtons();
  persistent_desks_bar_widget_->Show();
}

void PersistentDesksBarController::DestroyBarWidget() {
  persistent_desks_bar_widget_.reset();
  persistent_desks_bar_view_ = nullptr;
}

}  // namespace ash
