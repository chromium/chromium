// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

#include <memory>

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

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);
  Shelf* shelf = Shelf::ForWindow(root_window);
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<AppListBubbleView>(controller_, root_window,
                                          shelf->alignment()));
  bubble_widget_->AddObserver(this);
  bubble_widget_->Show();
  // TODO(https://crbug.com/1205494): Focus search box.

  // Set up event filter to close the bubble for clicks outside the bubble that
  // don't cause window activation changes (e.g. clicks on wallpaper or blank
  // areas of shelf).
  HomeButton* home_button = shelf->navigation_widget()->GetHomeButton();
  bubble_event_filter_ = std::make_unique<AppListBubbleEventFilter>(
      bubble_widget_, home_button,
      base::BindRepeating(&AppListBubblePresenter::Dismiss,
                          base::Unretained(this)));
}

void AppListBubblePresenter::Toggle(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (bubble_widget_) {
    Dismiss();
    return;
  }
  Show(display_id);
}

void AppListBubblePresenter::Dismiss() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (!bubble_widget_)
    return;
  bubble_widget_->CloseNow();
}

bool AppListBubblePresenter::IsShowing() const {
  return !!bubble_widget_;
}

void AppListBubblePresenter::OnWidgetDestroying(views::Widget* widget) {
  // `bubble_event_filter_` holds a pointer to the widget.
  bubble_event_filter_.reset();
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
}

}  // namespace ash
