// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_pinned_notification.h"
#include <string>

#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_system_tray_icon_unittest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

std::u16string GetExpectedMessage(
    const std::vector<HidSystemTrayIconTestBase::OriginItem>& origin_items) {
  auto sorted_origin_items = origin_items;
  // Sort the |origin_items| by origin. This is necessary because the origin
  // items for each profile in the pinned notification are created by iterating
  // through a structure of flat_map<url::Origin, ...>.
  base::ranges::sort(sorted_origin_items);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::vector<std::string> extension_names;
  for (const auto& [origin, connection_count, name] : sorted_origin_items) {
    CHECK(origin.scheme() == extensions::kExtensionScheme);
    extension_names.push_back(name);
  }
  CHECK(!extension_names.empty());
  if (extension_names.size() == 1) {
    return base::UTF8ToUTF16(base::StringPrintf("%s is accessing HID devices",
                                                extension_names[0].c_str()));
  } else if (extension_names.size() == 2) {
    return base::UTF8ToUTF16(base::StringPrintf(
        "Extensions accessing devices: %s, %s", extension_names[0].c_str(),
        extension_names[1].c_str()));
  }
  return base::UTF8ToUTF16(
      base::StringPrintf("Extensions accessing devices: %s, %s +%zu more",
                         extension_names[0].c_str(), extension_names[1].c_str(),
                         extension_names.size() - 2));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

}  // namespace

class HidPinnedNotificationTest : public HidSystemTrayIconTestBase {
 public:
  void SetUp() override {
    HidSystemTrayIconTestBase::SetUp();
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(nullptr);
    HidSystemTrayIconTestBase::TearDown();
  }

  void CheckIcon(const std::vector<HidSystemTrayIconTestBase::ProfileItem>&
                     profile_connection_counts) override {
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

      auto* hid_connection_tracker = static_cast<MockHidConnectionTracker*>(
          HidConnectionTrackerFactory::GetForProfile(profile,
                                                     /*create=*/false));
      EXPECT_TRUE(hid_connection_tracker);
      auto maybe_notification = display_service_->GetNotification(
          HidPinnedNotification::GetNotificationId(profile));
      ASSERT_TRUE(maybe_notification);
      EXPECT_EQ(maybe_notification->title(),
                GetExpectedTitle(origin_items.size(), total_connection_count));
      EXPECT_EQ(maybe_notification->message(),
                GetExpectedMessage(origin_items));
      EXPECT_EQ(maybe_notification->priority(), message_center::LOW_PRIORITY);
      EXPECT_EQ(maybe_notification->rich_notification_data().buttons.size(),
                1u);
      EXPECT_EQ(maybe_notification->rich_notification_data().buttons[0].title,
                u"HID settings");
      EXPECT_TRUE(maybe_notification->delegate());

      EXPECT_CALL(*hid_connection_tracker, ShowContentSettingsExceptions());
      SimulateButtonClick(profile);
    }
  }

  void CheckIconHidden() override {
    EXPECT_TRUE(display_service_
                    ->GetDisplayedNotificationsForType(
                        NotificationHandler::Type::TRANSIENT)
                    .empty());
  }

 private:
  void SimulateButtonClick(Profile* profile) {
    display_service_->SimulateClick(
        NotificationHandler::Type::TRANSIENT,
        HidPinnedNotification::GetNotificationId(profile),
        /*action_index=*/0, /*reply=*/absl::nullopt);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(HidPinnedNotificationTest, SingleProfileEmptyNameExtensionOrigins) {
  // Current TestingProfileManager can't support empty profile name as it uses
  // profile name for profile path. Passing empty would result in a failure in
  // ProfileManager::IsAllowedProfilePath(). Changing the way
  // TestingProfileManager creating profile path like adding "profile" prefix
  // doesn't work either as some tests are written in a way that takes
  // assumption of testing profile path pattern. Hence it creates testing
  // profile with non-empty name and then change the profile name to empty which
  // can still achieve what this file wants to test.
  profile()->set_profile_name("");
  TestSingleProfileExtentionOrigins();
}

TEST_F(HidPinnedNotificationTest, BounceConnectionExtensionOrigins) {
  TestBounceConnectionExtensionOrigins();
}

TEST_F(HidPinnedNotificationTest, SingleProfileNonEmptyNameExtentionOrigins) {
  TestSingleProfileExtentionOrigins();
}

TEST_F(HidPinnedNotificationTest, MultipleProfilesExtentionOrigins) {
  TestMultipleProfilesExtensionOrigins();
}

TEST_F(HidPinnedNotificationTest, ExtensionRemoval) {
  TestExtensionRemoval();
}

// Test message in pinned notification for up to 5 extensions.
TEST_F(HidPinnedNotificationTest, MultipleExtensionsNotificationMessage) {
  Profile* profile = CreateTestingProfile("user");
  HidConnectionTracker* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/true);

  std::vector<HidSystemTrayIconTestBase::OriginItem> origin_items;
  for (size_t idx = 0; idx < 5; idx++) {
    auto extension =
        CreateExtensionWithName(base::StringPrintf("Test Extension %zu", idx));
    AddExtensionToProfile(profile, extension.get());
    hid_connection_tracker->IncrementConnectionCount(extension.get()->origin());
    origin_items.emplace_back(extension->origin(), 1, extension->name());
    CheckIcon({{profile, origin_items}});
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
