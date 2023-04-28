// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_view_test_api.h"

#include "ash/app_menu/notification_item_view.h"
#include "ash/app_menu/notification_menu_header_view.h"
#include "ash/app_menu/notification_menu_view.h"
#include "ash/app_menu/notification_overflow_view.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ui/views/controls/label.h"

namespace ash {

NotificationMenuViewTestAPI::NotificationMenuViewTestAPI(
    NotificationMenuView* notification_menu_view)
    : notification_menu_view_(notification_menu_view) {}

NotificationMenuViewTestAPI::~NotificationMenuViewTestAPI() = default;

std::u16string NotificationMenuViewTestAPI::GetCounterViewContents() const {
  return notification_menu_view_->header_view_->counter_->GetText();
}

int NotificationMenuViewTestAPI::GetItemViewCount() const {
  return notification_menu_view_->notification_item_views_.size();
}

NotificationOverflowView* NotificationMenuViewTestAPI::GetOverflowView() const {
  return notification_menu_view_->overflow_view_;
}

}  // namespace ash
