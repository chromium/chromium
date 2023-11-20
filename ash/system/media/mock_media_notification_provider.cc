// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/mock_media_notification_provider.h"

#include "ui/views/view.h"

using ::testing::_;

namespace ash {

MockMediaNotificationProvider::MockMediaNotificationProvider()
    : old_provider_(MediaNotificationProvider::Get()) {
  MediaNotificationProvider::Set(this);

  ON_CALL(*this, GetMediaNotificationListView)
      .WillByDefault([](auto, auto, auto, const auto&) {
        return std::make_unique<views::View>();
      });
}

MockMediaNotificationProvider::~MockMediaNotificationProvider() {
  MediaNotificationProvider::Set(old_provider_);
}

bool MockMediaNotificationProvider::HasActiveNotifications() {
  return has_active_notifications_;
}

bool MockMediaNotificationProvider::HasFrozenNotifications() {
  return has_frozen_notifications_;
}

std::unique_ptr<global_media_controls::MediaItemUIDeviceSelector>
MockMediaNotificationProvider::BuildDeviceSelectorView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    bool show_devices) {
  return nullptr;
}

std::unique_ptr<global_media_controls::MediaItemUIFooter>
MockMediaNotificationProvider::BuildFooterView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  return nullptr;
}

void MockMediaNotificationProvider::SetHasActiveNotifications(
    bool has_active_notifications) {
  has_active_notifications_ = has_active_notifications;
}

void MockMediaNotificationProvider::SetHasFrozenNotifications(
    bool has_frozen_notifications) {
  has_frozen_notifications_ = has_frozen_notifications;
}

}  // namespace ash
