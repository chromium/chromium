// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_delegate_impl.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_event_filter.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Whether the shelf is oriented on the side, not on the bottom.
bool IsSideShelf(Shelf* shelf) {
  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return false;
    case ShelfAlignment::kLeft:
    case ShelfAlignment::kRight:
      return true;
  }
  return false;
}

// Whether the shelf background type indicates that shelf has rounded corners.
bool IsShelfBackgroundTypeWithRoundedCorners(
    ShelfBackgroundType background_type) {
  switch (background_type) {
    case ShelfBackgroundType::kDefaultBg:
    case ShelfBackgroundType::kAppList:
    case ShelfBackgroundType::kOverview:
      return true;
    case ShelfBackgroundType::kMaximized:
    case ShelfBackgroundType::kMaximizedWithAppList:
    case ShelfBackgroundType::kOobe:
    case ShelfBackgroundType::kHomeLauncher:
    case ShelfBackgroundType::kLogin:
    case ShelfBackgroundType::kLoginNonBlurredWallpaper:
    case ShelfBackgroundType::kInApp:
      return false;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, public:

AppListPresenterDelegateImpl::AppListPresenterDelegateImpl(
    AppListControllerImpl* controller)
    : controller_(controller) {
  display_observation_.Observe(display::Screen::GetScreen());
}

AppListPresenterDelegateImpl::~AppListPresenterDelegateImpl() = default;

void AppListPresenterDelegateImpl::SetPresenter(
    AppListPresenterImpl* presenter) {
  presenter_ = presenter;
}

void AppListPresenterDelegateImpl::SetView(AppListView* view) {
  DCHECK(view);
  view_ = view;
}

void AppListPresenterDelegateImpl::ShowForDisplay(
    AppListViewState preferred_state,
    int64_t display_id) {
  DCHECK(view_);

  is_visible_ = true;

  controller_->UpdateLauncherContainer(display_id);

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shelf* shelf =
      Shelf::ForWindow(view_->GetWidget()->GetNativeView()->GetRootWindow());
  shelf->shelf_layout_manager()->UpdateAutoHideState();

  // Observe the shelf for changes to rounded corners.
  if (!shelf_observation_.IsObservingSource(shelf))
    shelf_observation_.AddObservation(shelf);

  // By setting us as a drag-and-drop recipient, the app list knows that we can
  // handle items. Do this on every show because |view_| can be reused after a
  // monitor is disconnected but that monitor's ShelfView and
  // ScrollableShelfView are deleted. https://crbug.com/1163332
  view_->SetDragAndDropHostOfCurrentAppList(
      shelf->shelf_widget()->GetDragAndDropHostForAppList());
  view_->SetShelfHasRoundedCorners(
      IsShelfBackgroundTypeWithRoundedCorners(shelf->GetBackgroundType()));
  view_->Show(preferred_state, IsSideShelf(shelf));

  SnapAppListBoundsToDisplayEdge();

  event_filter_ = std::make_unique<AppListPresenterEventFilter>(
      controller_, presenter_, view_);
  controller_->ViewShown(display_id);
}

void AppListPresenterDelegateImpl::OnClosing() {
  DCHECK(is_visible_);
  DCHECK(view_);
  is_visible_ = false;
  event_filter_.reset();
  controller_->ViewClosing();
}

void AppListPresenterDelegateImpl::OnClosed() {
  if (!is_visible_)
    shelf_observation_.RemoveAllObservations();
  controller_->ViewClosed();
}

void AppListPresenterDelegateImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!presenter_->GetWindow())
    return;

  view_->OnParentWindowBoundsChanged();
  SnapAppListBoundsToDisplayEdge();
}

void AppListPresenterDelegateImpl::OnBackgroundTypeChanged(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  view_->SetShelfHasRoundedCorners(
      IsShelfBackgroundTypeWithRoundedCorners(background_type));
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, private:

void AppListPresenterDelegateImpl::SnapAppListBoundsToDisplayEdge() {
  CHECK(view_ && view_->GetWidget());
  aura::Window* window = view_->GetWidget()->GetNativeView();
  const gfx::Rect bounds =
      controller_->SnapBoundsToDisplayEdge(window->bounds());
  window->SetBounds(bounds);
}

}  // namespace ash
