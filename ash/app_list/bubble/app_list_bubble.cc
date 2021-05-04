// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble.h"

#include <memory>

#include "ash/app_list/bubble/app_list_bubble_view.h"
#include "base/logging.h"
#include "ui/views/widget/widget.h"

namespace ash {

AppListBubble::AppListBubble() = default;

AppListBubble::~AppListBubble() = default;

void AppListBubble::Show(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (bubble_widget_)
    return;
  bubble_widget_ =
      base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
          std::make_unique<AppListBubbleView>()));
  bubble_widget_->Show();
  // TODO(https://crbug.com/1205494): Focus search box.
}

void AppListBubble::Toggle(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (bubble_widget_) {
    Dismiss();
    return;
  }
  Show(display_id);
}

void AppListBubble::Dismiss() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  bubble_widget_.reset();  // Triggers asynchronous close.
}

bool AppListBubble::IsShowing() const {
  return !!bubble_widget_;
}

}  // namespace ash
