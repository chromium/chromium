// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MOCK_MEDIA_NOTIFICATION_PROVIDER_H_
#define ASH_SYSTEM_MEDIA_MOCK_MEDIA_NOTIFICATION_PROVIDER_H_

#include "ash/system/media/media_notification_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class MockMediaNotificationProvider : public MediaNotificationProvider {
 public:
  MockMediaNotificationProvider();
  MockMediaNotificationProvider(const MockMediaNotificationProvider&) = delete;
  MockMediaNotificationProvider& operator=(
      const MockMediaNotificationProvider&) = delete;
  ~MockMediaNotificationProvider() override;

  // MediaNotificationProvider:
  MOCK_METHOD((std::unique_ptr<views::View>),
              GetMediaNotificationListView,
              (int,
               bool,
               global_media_controls::GlobalMediaControlsEntryPoint,
               const std::string&));
  MOCK_METHOD(void, OnBubbleClosing, ());
  MOCK_METHOD(global_media_controls::MediaItemManager*,
              GetMediaItemManager,
              ());

  void AddObserver(MediaNotificationProviderObserver* observer) override {}
  void RemoveObserver(MediaNotificationProviderObserver* observer) override {}
  bool HasActiveNotifications() override;
  bool HasFrozenNotifications() override;
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override {}
  std::unique_ptr<global_media_controls::MediaItemUIDeviceSelector>
  BuildDeviceSelectorView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      bool show_devices) override;
  std::unique_ptr<global_media_controls::MediaItemUIFooter> BuildFooterView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;

  void SetHasActiveNotifications(bool has_active_notifications);
  void SetHasFrozenNotifications(bool has_frozen_notifications);

 private:
  bool has_active_notifications_ = false;
  bool has_frozen_notifications_ = false;
  const raw_ptr<MediaNotificationProvider> old_provider_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MOCK_MEDIA_NOTIFICATION_PROVIDER_H_
