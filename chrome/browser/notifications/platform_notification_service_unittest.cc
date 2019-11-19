// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/metrics/mock_notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using blink::NotificationResources;
using blink::PlatformNotificationData;
using content::NotificationDatabaseData;
using message_center::Notification;

namespace {

const int kNotificationVibrationPattern[] = { 100, 200, 300 };
const char kNotificationId[] = "my-notification-id";
const char kClosedReason[] = "ClosedReason";
const char kDidReplaceAnotherNotification[] = "DidReplaceAnotherNotification";
const char kHasBadge[] = "HasBadge";
const char kHasImage[] = "HasImage";
const char kHasIcon[] = "HasIcon";
const char kHasRenotify[] = "HasRenotify";
const char kHasTag[] = "HasTag";
const char kIsSilent[] = "IsSilent";
const char kNumActions[] = "NumActions";
const char kNumClicks[] = "NumClicks";
const char kNumActionButtonClicks[] = "NumActionButtonClicks";
const char kRequireInteraction[] = "RequireInteraction";
const char kTimeUntilCloseMillis[] = "TimeUntilClose";
const char kTimeUntilFirstClickMillis[] = "TimeUntilFirstClick";
const char kTimeUntilLastClickMillis[] = "TimeUntilLastClick";
const GURL kOrigin = GURL("https://example.com");

}  // namespace

class PlatformNotificationServiceTest : public testing::Test {
 public:
  void SetUp() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(&profile_);

    mock_logger_ = static_cast<MockNotificationMetricsLogger*>(
        NotificationMetricsLoggerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_,
                base::BindRepeating(
                    &MockNotificationMetricsLogger::FactoryForTests)));

    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDown() override {
    display_service_tester_.reset();
  }

 protected:
  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() {
    return PlatformNotificationServiceFactory::GetForProfile(&profile_);
  }

  size_t GetNotificationCountForType(NotificationHandler::Type type) {
    return display_service_tester_->GetDisplayedNotificationsForType(type)
        .size();
  }

  Notification GetDisplayedNotificationForType(NotificationHandler::Type type) {
    std::vector<Notification> notifications =
        display_service_tester_->GetDisplayedNotificationsForType(type);
    DCHECK_EQ(1u, notifications.size());

    return notifications[0];
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  // Owned by the |profile_| as a keyed service.
  MockNotificationMetricsLogger* mock_logger_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;
};

TEST_F(PlatformNotificationServiceTest, DisplayNonPersistentThenClose) {
  PlatformNotificationData data;
  data.title = base::ASCIIToUTF16("My Notification");
  data.body = base::ASCIIToUTF16("Hello, world!");

  service()->DisplayNotification(kNotificationId, GURL("https://chrome.com/"),
                                 data, NotificationResources());

  EXPECT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));

  service()->CloseNotification(kNotificationId);

  EXPECT_EQ(0u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));
}

TEST_F(PlatformNotificationServiceTest, DisplayPersistentThenClose) {
  PlatformNotificationData data;
  data.title = base::ASCIIToUTF16("My notification's title");
  data.body = base::ASCIIToUTF16("Hello, world!");

  EXPECT_CALL(*mock_logger_, LogPersistentNotificationShown());
  service()->DisplayPersistentNotification(
      kNotificationId, GURL() /* service_worker_scope */,
      GURL("https://chrome.com/"), data, NotificationResources());

  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));

  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_PERSISTENT);
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title",
      base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!",
      base::UTF16ToUTF8(notification.message()));

  service()->ClosePersistentNotification(kNotificationId);
  EXPECT_EQ(0u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));
}

TEST_F(PlatformNotificationServiceTest, DisplayNonPersistentPropertiesMatch) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern +
          base::size(kNotificationVibrationPattern));

  PlatformNotificationData data;
  data.title = base::ASCIIToUTF16("My notification's title");
  data.body = base::ASCIIToUTF16("Hello, world!");
  data.vibration_pattern = vibration_pattern;
  data.silent = true;

  service()->DisplayNotification(kNotificationId, GURL("https://chrome.com/"),
                                 data, NotificationResources());

  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));

  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_NON_PERSISTENT);
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title",
      base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!",
      base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
      testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_TRUE(notification.silent());
}

TEST_F(PlatformNotificationServiceTest, DisplayPersistentPropertiesMatch) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern +
          base::size(kNotificationVibrationPattern));

  PlatformNotificationData data;
  data.title = base::ASCIIToUTF16("My notification's title");
  data.body = base::ASCIIToUTF16("Hello, world!");
  data.vibration_pattern = vibration_pattern;
  data.silent = true;
  data.actions.resize(2);
  data.actions[0].type = blink::PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
  data.actions[0].title = base::ASCIIToUTF16("Button 1");
  data.actions[1].type = blink::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
  data.actions[1].title = base::ASCIIToUTF16("Button 2");

  NotificationResources notification_resources;
  notification_resources.action_icons.resize(data.actions.size());

  EXPECT_CALL(*mock_logger_, LogPersistentNotificationShown());
  service()->DisplayPersistentNotification(
      kNotificationId, GURL() /* service_worker_scope */,
      GURL("https://chrome.com/"), data, notification_resources);

  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));

  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_PERSISTENT);
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!", base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
              testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_TRUE(notification.silent());

  const auto& buttons = notification.buttons();
  ASSERT_EQ(2u, buttons.size());
  EXPECT_EQ("Button 1", base::UTF16ToUTF8(buttons[0].title));
  EXPECT_FALSE(buttons[0].placeholder);
  EXPECT_EQ("Button 2", base::UTF16ToUTF8(buttons[1].title));
  EXPECT_TRUE(buttons[1].placeholder);
}

TEST_F(PlatformNotificationServiceTest, RecordNotificationUkmEvent) {
  // Set up UKM recording conditions.
  ASSERT_TRUE(profile_.CreateHistoryService(/* delete_file= */ true,
                                            /* no_db= */ false));
  auto* history_service = HistoryServiceFactory::GetForProfile(
      &profile_, ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(kOrigin, base::Time::Now(), history::SOURCE_BROWSED);

  NotificationDatabaseData data;
  data.notification_id = "notification1";
  data.origin = kOrigin;
  data.closed_reason = NotificationDatabaseData::ClosedReason::USER;
  data.replaced_existing_notification = true;
  data.notification_data.icon = GURL("https://icon.com");
  data.notification_data.image = GURL("https://image.com");
  data.notification_data.renotify = false;
  data.notification_data.tag = "tag";
  data.notification_data.silent = true;
  blink::PlatformNotificationAction action1, action2, action3;
  data.notification_data.actions.push_back(action1);
  data.notification_data.actions.push_back(action2);
  data.notification_data.actions.push_back(action3);
  data.notification_data.require_interaction = false;
  data.num_clicks = 3;
  data.num_action_button_clicks = 1;
  data.time_until_close_millis = base::TimeDelta::FromMilliseconds(10000);
  data.time_until_first_click_millis = base::TimeDelta::FromMilliseconds(2222);
  data.time_until_last_click_millis = base::TimeDelta::FromMilliseconds(3333);

  // Initially there are no UKM entries.
  std::vector<const ukm::mojom::UkmEntry*> entries =
      recorder_->GetEntriesByName(ukm::builders::Notification::kEntryName);
  EXPECT_EQ(0u, entries.size());

  {
    base::RunLoop run_loop;
    service()->set_ukm_recorded_closure_for_testing(run_loop.QuitClosure());
    service()->RecordNotificationUkmEvent(data);
    run_loop.Run();
  }

  entries =
      recorder_->GetEntriesByName(ukm::builders::Notification::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries[0];
  recorder_->ExpectEntryMetric(
      entry, kClosedReason,
      static_cast<int>(NotificationDatabaseData::ClosedReason::USER));
  recorder_->ExpectEntryMetric(entry, kDidReplaceAnotherNotification, true);
  recorder_->ExpectEntryMetric(entry, kHasBadge, false);
  recorder_->ExpectEntryMetric(entry, kHasIcon, 1);
  recorder_->ExpectEntryMetric(entry, kHasImage, 1);
  recorder_->ExpectEntryMetric(entry, kHasRenotify, false);
  recorder_->ExpectEntryMetric(entry, kHasTag, true);
  recorder_->ExpectEntryMetric(entry, kIsSilent, 1);
  recorder_->ExpectEntryMetric(entry, kNumActions, 3);
  recorder_->ExpectEntryMetric(entry, kNumActionButtonClicks, 1);
  recorder_->ExpectEntryMetric(entry, kNumClicks, 3);
  recorder_->ExpectEntryMetric(entry, kRequireInteraction, false);
  recorder_->ExpectEntryMetric(entry, kTimeUntilCloseMillis, 10000);
  recorder_->ExpectEntryMetric(entry, kTimeUntilFirstClickMillis, 2222);
  recorder_->ExpectEntryMetric(entry, kTimeUntilLastClickMillis, 3333);
}

// Expect each call to ReadNextPersistentNotificationId to return a larger
// value.
TEST_F(PlatformNotificationServiceTest, NextPersistentNotificationId) {
  int64_t first_id = service()->ReadNextPersistentNotificationId();
  int64_t second_id = service()->ReadNextPersistentNotificationId();
  EXPECT_LT(first_id, second_id);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(PlatformNotificationServiceTest, DisplayNameForContextMessage) {
  base::string16 display_name =
      service()->DisplayNameForContextMessage(GURL("https://chrome.com/"));

  EXPECT_TRUE(display_name.empty());

  // Create a mocked extension.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetID("honijodknafkokifofgiaalefdiedpko")
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "NotificationTest")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Test Extension")
                           .Build())
          .Build();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(&profile_);
  EXPECT_TRUE(registry->AddEnabled(extension));

  display_name = service()->DisplayNameForContextMessage(
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"));
  EXPECT_EQ("NotificationTest", base::UTF16ToUTF8(display_name));
}

TEST_F(PlatformNotificationServiceTest, CreateNotificationFromData) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My Notification");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");
  GURL origin("https://chrome.com/");

  Notification notification = service()->CreateNotificationFromData(
      origin, "id", notification_data, NotificationResources());
  EXPECT_TRUE(notification.context_message().empty());

  // Create a mocked extension.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetID("honijodknafkokifofgiaalefdiedpko")
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "NotificationTest")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Test Extension")
                           .Build())
          .Build();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(&profile_);
  EXPECT_TRUE(registry->AddEnabled(extension));

  notification = service()->CreateNotificationFromData(
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"),
      "id", notification_data, NotificationResources());
  EXPECT_EQ("NotificationTest",
            base::UTF16ToUTF8(notification.context_message()));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
