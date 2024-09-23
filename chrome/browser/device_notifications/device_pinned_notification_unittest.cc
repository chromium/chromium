// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_pinned_notification_unittest.h"

#include <string>

#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

DevicePinnedNotificationTestBase::DevicePinnedNotificationTestBase(
    std::u16string device_content_settings_label)
    : device_content_settings_label_(std::move(device_content_settings_label)) {
}

DevicePinnedNotificationTestBase::~DevicePinnedNotificationTestBase() = default;

void DevicePinnedNotificationTestBase::SetUp() {
  DeviceSystemTrayIconTestBase::SetUp();
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
      std::make_unique<SystemNotificationHelper>());
  display_service_ =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
}

void DevicePinnedNotificationTestBase::TearDown() {
  TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(nullptr);
  DeviceSystemTrayIconTestBase::TearDown();
}

void DevicePinnedNotificationTestBase::CheckIcon(
    const std::vector<DeviceSystemTrayIconTestBase::ProfileItem>&
        profile_connection_counts) {
  EXPECT_FALSE(display_service_
                   ->GetDisplayedNotificationsForType(
                       NotificationHandler::Type::TRANSIENT)
                   .empty());

  // Check each button label and behavior of clicking the button.
  for (const auto& [profile, origin_items] : profile_connection_counts) {
    size_t total_connection_count = 0;
    for (const auto& [origin, connection_count, name] : origin_items) {
      total_connection_count += connection_count;
    }

    auto* device_pinned_notification_renderer =
        GetDevicePinnedNotificationRenderer();
    auto* connection_tracker = GetDeviceConnectionTracker(profile,
                                                          /*create=*/false);

    ASSERT_TRUE(connection_tracker);
    auto maybe_notification = display_service_->GetNotification(
        device_pinned_notification_renderer->GetNotificationId(profile));
    ASSERT_TRUE(maybe_notification);
    EXPECT_EQ(maybe_notification->title(),
              GetExpectedTitle(origin_items.size(), total_connection_count));
    EXPECT_EQ(maybe_notification->message(), GetExpectedMessage(origin_items));
    EXPECT_EQ(maybe_notification->priority(), message_center::LOW_PRIORITY);
    ASSERT_EQ(maybe_notification->rich_notification_data().buttons.size(), 1u);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!ash::features::AreOngoingProcessesEnabled()) {
      EXPECT_EQ(maybe_notification->rich_notification_data().buttons[0].title,
                device_content_settings_label_);
    }
#else
    EXPECT_EQ(maybe_notification->rich_notification_data().buttons[0].title,
              device_content_settings_label_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_TRUE(maybe_notification->delegate());

    EXPECT_CALL(*GetMockDeviceConnectionTracker(connection_tracker),
                ShowContentSettingsExceptions());
    SimulateButtonClick(profile);
  }
}

void DevicePinnedNotificationTestBase::CheckIconHidden() {
  EXPECT_TRUE(display_service_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .empty());
}

void DevicePinnedNotificationTestBase::SimulateButtonClick(Profile* profile) {
  auto* device_pinned_notification_renderer =
      GetDevicePinnedNotificationRenderer();
  display_service_->SimulateClick(
      NotificationHandler::Type::TRANSIENT,
      device_pinned_notification_renderer->GetNotificationId(profile),
      /*action_index=*/0, /*reply=*/std::nullopt);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Test message in pinned notification for up to 5 extensions.
void DevicePinnedNotificationTestBase::
    TestMultipleExtensionsNotificationMessage() {
  Profile* profile = CreateTestingProfile("user");
  auto* connection_tracker = GetDeviceConnectionTracker(profile,
                                                        /*create=*/true);

  std::vector<DeviceSystemTrayIconTestBase::OriginItem> origin_items;
  for (size_t idx = 0; idx < 5; idx++) {
    auto extension =
        CreateExtensionWithName(base::StringPrintf("Test Extension %zu", idx));
    AddExtensionToProfile(profile, extension.get());
    connection_tracker->IncrementConnectionCount(extension.get()->origin());
    origin_items.emplace_back(extension->origin(), 1, extension->name());
    CheckIcon({{profile, origin_items}});
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
