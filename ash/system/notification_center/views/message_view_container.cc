// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/message_view_container.h"

#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr int kNotificationOuterCornerRadius =
    kMessageCenterScrollViewCornerRadius;
constexpr int kNotificationInnerCornerRadius =
    kMessageCenterNotificationInnerCornerRadius;

}  // namespace

MessageViewContainer::MessageViewContainer(
    std::unique_ptr<message_center::MessageView> message_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  message_view_ = AddChildView(std::move(message_view));
  message_view_->SetPreferredSize(CalculatePreferredSize());
}

int MessageViewContainer::CalculateHeight() const {
  return message_view_ ? message_view_->GetHeightForWidth(
                             kNotificationInMessageCenterWidth)
                       : 0;
}

void MessageViewContainer::UpdateBorder(const bool is_top,
                                        const bool is_bottom,
                                        const bool force_update) {
  if (is_top_ == is_top && is_bottom_ == is_bottom && !force_update) {
    return;
  }

  is_top_ = is_top;
  is_bottom_ = is_bottom;

  int top_radius =
      is_top ? kNotificationOuterCornerRadius : kNotificationInnerCornerRadius;
  int bottom_radius = is_bottom ? kNotificationOuterCornerRadius
                                : kNotificationInnerCornerRadius;

  message_view_->UpdateCornerRadius(top_radius, bottom_radius);

  // Do not set background for arc notifications since they have their own
  // custom background logic.
  if (!features::IsRenderArcNotificationsByChromeEnabled() &&
      !message_center_utils::IsAshNotificationView(message_view_)) {
    return;
  }

  message_view_->SetBackground(
      notification_style_utils::CreateNotificationBackground(
          top_radius, bottom_radius, /*is_popup_notification=*/false,
          /*is_grouped_child_notification=*/false));
}

const std::string MessageViewContainer::GetNotificationId() const {
  return message_view_->notification_id();
}

void MessageViewContainer::UpdateWithNotification(
    const message_center::Notification& notification) {
  message_view_->UpdateWithNotification(notification);
}

gfx::Size MessageViewContainer::CalculatePreferredSize() const {
  return gfx::Size(kNotificationInMessageCenterWidth, CalculateHeight());
}

BEGIN_METADATA(MessageViewContainer);
END_METADATA

}  // namespace ash
