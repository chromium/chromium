// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/bubble/app_list_bubble_event_filter.h"
#include "ash/app_list/bubble/app_list_bubble_view.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

AppListBubblePresenter::AppListBubblePresenter(
    AppListControllerImpl* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

AppListBubblePresenter::~AppListBubblePresenter() {
  if (bubble_widget_)
    bubble_widget_->CloseNow();
  CHECK(!IsInObserverList());
}

void AppListBubblePresenter::Show(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (bubble_widget_)
    return;

  base::Time time_shown = base::Time::Now();

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);
  Shelf* shelf = Shelf::ForWindow(root_window);
  auto bubble_view = std::make_unique<AppListBubbleView>(
      controller_, root_window, shelf->alignment());
  bubble_view_ = bubble_view.get();
  bubble_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  bubble_widget_->AddObserver(this);
  controller_->OnVisibilityWillChange(/*visible=*/true, display_id);
  bubble_widget_->Show();
  controller_->OnVisibilityChanged(/*visible=*/true, display_id);
  bubble_view_->FocusSearchBox();  // Must happen after widget creation.

  // Bubble launcher is always keyboard traversable.
  controller_->SetKeyboardTraversalMode(true);

  // Set up event filter to close the bubble for clicks outside the bubble that
  // don't cause window activation changes (e.g. clicks on wallpaper or blank
  // areas of shelf).
  HomeButton* home_button = shelf->navigation_widget()->GetHomeButton();
  bubble_event_filter_ = std::make_unique<AppListBubbleEventFilter>(
      bubble_widget_, home_button,
      base::BindRepeating(&AppListBubblePresenter::OnPressOutsideBubble,
                          base::Unretained(this)));

  UmaHistogramTimes("Apps.AppListBubbleCreationTime",
                    base::Time::Now() - time_shown);
}

ShelfAction AppListBubblePresenter::Toggle(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (bubble_widget_) {
    Dismiss();
    return SHELF_ACTION_APP_LIST_DISMISSED;
  }
  Show(display_id);
  return SHELF_ACTION_APP_LIST_SHOWN;
}

void AppListBubblePresenter::Dismiss() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (!bubble_widget_)
    return;

  // Reset keyboard traversal in case the user switches to tablet launcher.
  // Must happen before widget is destroyed.
  controller_->SetKeyboardTraversalMode(false);

  const int64_t display_id = GetDisplayId();
  controller_->OnVisibilityWillChange(/*visible=*/false, display_id);
  bubble_widget_->CloseNow();
  controller_->OnVisibilityChanged(/*visible=*/false, display_id);
}

bool AppListBubblePresenter::IsShowing() const {
  return !!bubble_widget_;
}

void AppListBubblePresenter::OnWidgetDestroying(views::Widget* widget) {
  // `bubble_event_filter_` holds a pointer to the widget.
  bubble_event_filter_.reset();
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  bubble_view_ = nullptr;
}

void AppListBubblePresenter::OnPressOutsideBubble() {
  // Presses outside the bubble could be activating a shelf item. Record the
  // AppListBubble state prior to dismissal.
  controller_->RecordAppListState();
  Dismiss();
}

int64_t AppListBubblePresenter::GetDisplayId() const {
  if (!bubble_widget_)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(bubble_widget_->GetNativeView())
      .id();
}

}  // namespace ash
