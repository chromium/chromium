// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_PINNED_NOTIFICATION_UNITTEST_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_PINNED_NOTIFICATION_UNITTEST_H_

#include <string>

#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_unittest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"

class DevicePinnedNotificationTestBase : public DeviceSystemTrayIconTestBase {
 public:
  explicit DevicePinnedNotificationTestBase(
      std::u16string device_content_settings_label);
  ~DevicePinnedNotificationTestBase() override;

  void SetUp() override;
  void TearDown() override;
  void CheckIcon(const std::vector<DeviceSystemTrayIconTestBase::ProfileItem>&
                     profile_connection_counts) override;
  void CheckIconHidden() override;

  virtual DevicePinnedNotificationRenderer*
  GetDevicePinnedNotificationRenderer() = 0;

  // Get the expected message shown in the pinned notification.
  virtual std::u16string GetExpectedMessage(
      const std::vector<DeviceSystemTrayIconTestBase::OriginItem>&
          origin_items) = 0;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void TestMultipleExtensionsNotificationMessage();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

 private:
  void SimulateButtonClick(Profile* profile);

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::u16string device_content_settings_label_;
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_PINNED_NOTIFICATION_UNITTEST_H_
