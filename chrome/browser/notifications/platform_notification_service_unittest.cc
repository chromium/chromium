// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/metrics/mock_notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/safe_browsing/notification_content_detection/mock_notification_content_detection_service.h"
#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_service_factory.h"
#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#endif

using blink::NotificationResources;
using blink::PlatformNotificationData;
using content::NotificationDatabaseData;
using message_center::Notification;
using ::testing::_;

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

}  // namespace

class PlatformNotificationServiceTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {safe_browsing::kReportNotificationContentDetectionData,
             safe_browsing::kAutoRevokeSuspiciousNotification});
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());

    mock_logger_ = static_cast<MockNotificationMetricsLogger*>(
        NotificationMetricsLoggerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating(
                    &MockNotificationMetricsLogger::FactoryForTests)));

    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDown() override {
    display_service_tester_.reset();
    mock_logger_ = nullptr;
    profile_.reset();
  }

  content::PlatformNotificationContext* GetPlatformNotificationContext(
      GURL origin) {
    return profile_->GetStoragePartitionForUrl(origin)
        ->GetPlatformNotificationContext();
  }

  NotificationDatabaseData ReadNotificationDataAndRecordInteractionSynchronous(
      content::PlatformNotificationContext* context,
      const std::string& notification_id,
      const GURL& origin) {
    base::RunLoop run_loop;
    context->ReadNotificationDataAndRecordInteraction(
        "p#" + origin.spec() + "#0" + notification_id, origin,
        content::PlatformNotificationContext::Interaction::NONE,
        base::BindOnce(
            &PlatformNotificationServiceTest::
                DidReadNotificationDataAndRecordInteractionSynchronous,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return notification_database_data_;
  }

  void DidReadNotificationDataAndRecordInteractionSynchronous(
      base::OnceClosure quit_closure,
      bool success,
      const NotificationDatabaseData& data) {
    ASSERT_TRUE(success);
    notification_database_data_ = data;
    std::move(quit_closure).Run();
  }

 protected:
  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() {
    return PlatformNotificationServiceFactory::GetForProfile(profile_.get());
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
  // This needs to be declared before `task_environment_`, so that it is
  // destroyed after `task_environment_` - this ensures that tasks running on
  // other threads won't access `scoped_feature_list_` while it is being
  // destroyed.
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  // Owned by the |profile_| as a keyed service.
  raw_ptr<MockNotificationMetricsLogger> mock_logger_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;

 private:
  NotificationDatabaseData notification_database_data_;
};

TEST_F(PlatformNotificationServiceTest, DisplayNonPersistentThenClose) {
  PlatformNotificationData data;
  data.title = u"My Notification";
  data.body = u"Hello, world!";

  service()->DisplayNotification(kNotificationId, GURL("https://chrome.com/"),
                                 /*document_url=*/GURL(), data,
                                 NotificationResources());

  EXPECT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));

  service()->CloseNotification(kNotificationId);

  EXPECT_EQ(0u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));
}

TEST_F(PlatformNotificationServiceTest, DisplayPersistentThenClose) {
  PlatformNotificationData data;
  data.title = u"My notification's title";
  data.body = u"Hello, world!";

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
  std::vector<int> vibration_pattern(std::begin(kNotificationVibrationPattern),
                                     std::end(kNotificationVibrationPattern));

  PlatformNotificationData data;
  data.title = u"My notification's title";
  data.body = u"Hello, world!";
  data.vibration_pattern = vibration_pattern;
  data.silent = true;

  service()->DisplayNotification(kNotificationId, GURL("https://chrome.com/"),
                                 /*document_url=*/GURL(), data,
                                 NotificationResources());

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
  std::vector<int> vibration_pattern(std::begin(kNotificationVibrationPattern),
                                     std::end(kNotificationVibrationPattern));
  PlatformNotificationData data;
  data.title = u"My notification's title";
  data.body = u"Hello, world!";
  data.vibration_pattern = vibration_pattern;
  data.silent = true;
  data.actions.resize(2);
  data.actions[0] = blink::mojom::NotificationAction::New();
  data.actions[0]->type = blink::mojom::NotificationActionType::BUTTON;
  data.actions[0]->title = u"Button 1";
  data.actions[1] = blink::mojom::NotificationAction::New();
  data.actions[1]->type = blink::mojom::NotificationActionType::TEXT;
  data.actions[1]->title = u"Button 2";

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
  NotificationDatabaseData data;
  data.notification_id = "notification1";
  data.origin = GURL("https://example.com");
  data.closed_reason = NotificationDatabaseData::ClosedReason::USER;
  data.replaced_existing_notification = true;
  data.notification_data.icon = GURL("https://icon.com");
  data.notification_data.image = GURL("https://image.com");
  data.notification_data.renotify = false;
  data.notification_data.tag = "tag";
  data.notification_data.silent = true;
  auto action1 = blink::mojom::NotificationAction::New();
  data.notification_data.actions.push_back(std::move(action1));
  auto action2 = blink::mojom::NotificationAction::New();
  data.notification_data.actions.push_back(std::move(action2));
  auto action3 = blink::mojom::NotificationAction::New();
  data.notification_data.actions.push_back(std::move(action3));
  data.notification_data.require_interaction = false;
  data.num_clicks = 3;
  data.num_action_button_clicks = 1;
  data.time_until_close_millis = base::Milliseconds(10000);
  data.time_until_first_click_millis = base::Milliseconds(2222);
  data.time_until_last_click_millis = base::Milliseconds(3333);

  // Set up UKM recording conditions.
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(data.origin, base::Time::Now(),
                           history::SOURCE_BROWSED);

  // Initially there are no UKM entries.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
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
  auto* entry = entries[0].get();
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

TEST_F(PlatformNotificationServiceTest,
       ProposedDisruptiveNotificationRevocationMetricsPersistent) {
  GURL url("https://chrome.test/");
  const int kDailyNotificationCount = 4;

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  DisruptiveNotificationPermissionsManager::RevocationEntry entry(
      /*revocation_state=*/DisruptiveNotificationPermissionsManager::
          RevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/kDailyNotificationCount);

  DisruptiveNotificationPermissionsManager::ContentSettingHelper(*hcsm)
      .PersistRevocationEntry(url, entry);

  PlatformNotificationData data;
  data.title = u"My notification's title";
  data.body = u"Hello, world!";

  service()->DisplayPersistentNotification(kNotificationId,
                                           GURL() /* service_worker_scope */,
                                           url, data, NotificationResources());

  EXPECT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));
  // Check that the correct metric is reported.
  EXPECT_EQ(1u, recorder_
                    ->GetEntriesByName(
                        "SafetyHub.DisruptiveNotificationRevocations.Proposed")
                    .size());
}

TEST_F(PlatformNotificationServiceTest,
       ProposedDisruptiveNotificationRevocationMetricsNonPersistent) {
  GURL url("https://chrome.test/");
  const int kDailyNotificationCount = 4;

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  DisruptiveNotificationPermissionsManager::RevocationEntry entry(
      /*revocation_state=*/DisruptiveNotificationPermissionsManager::
          RevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/kDailyNotificationCount);

  DisruptiveNotificationPermissionsManager::ContentSettingHelper(*hcsm)
      .PersistRevocationEntry(url, entry);

  PlatformNotificationData data;
  data.title = u"My notification's title";
  data.body = u"Hello, world!";

  service()->DisplayNotification(kNotificationId, url,
                                 /*document_url=*/GURL(), data,
                                 NotificationResources());

  EXPECT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));
  // Check that the correct metric is reported.
  EXPECT_EQ(1u, recorder_
                    ->GetEntriesByName(
                        "SafetyHub.DisruptiveNotificationRevocations.Proposed")
                    .size());
}

#if !BUILDFLAG(IS_ANDROID)

class PlatformNotificationServiceTest_WebApps
    : public PlatformNotificationServiceTest {
 public:
  void SetUp() override {
    PlatformNotificationServiceTest::SetUp();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());

    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForTest(profile_.get());

    std::unique_ptr<web_app::WebAppInstallInfo> web_app =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            kWebAppStartUrl);
    web_app->title = u"Test app 1";
    installed_app_id =
        web_app::test::InstallWebApp(profile_.get(), std::move(web_app));

    web_app = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        kInstalledNestedWebAppStartUrl);
    web_app->title = u"Test app 2";
    nested_installed_app_id =
        web_app::test::InstallWebApp(profile_.get(), std::move(web_app));

    web_app = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        kNotInstalledNestedWebAppStartUrl);
    web_app->title = u"Test app 3";
    web_app::WebAppInstallParams params;
    params.install_state =
        web_app::proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
    // OS Hooks must be disabled for non-locally installed app.
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;

    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        future;
    provider->scheduler().InstallFromInfoWithParams(
        std::move(web_app), /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        future.GetCallback(), params);
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(future.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
  }

 protected:
  const GURL kWebAppOrigin = GURL("https://example.com");
  const GURL kWebAppStartUrl = GURL("https://example.com/scope/start.html");
  const GURL kInstalledNestedWebAppStartUrl =
      GURL("https://example.com/scope/other/foo.html");
  const GURL kNotInstalledNestedWebAppStartUrl =
      GURL("https://example.com/scope/nestedscope/foo.html");
  const GURL kOutOfScopeUrl = GURL("https://example.com/outofscope");
  const GURL kServiceWorkerScope = GURL("https://example.com/scope/");

  static PlatformNotificationData CreateDummyNotificationData() {
    PlatformNotificationData data;
    data.title = u"My Notification";
    data.body = u"Hello, world!";
    return data;
  }
  const PlatformNotificationData kNotificationData =
      CreateDummyNotificationData();

  webapps::AppId installed_app_id;
  webapps::AppId nested_installed_app_id;

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration fake_os_integration_;
};

TEST_F(PlatformNotificationServiceTest_WebApps, IncomingCallWebApp) {
  EXPECT_TRUE(service()->IsActivelyInstalledWebAppScope(kWebAppStartUrl));

  web_app::test::UninstallAllWebApps(profile_.get());
  EXPECT_FALSE(service()->IsActivelyInstalledWebAppScope(kWebAppStartUrl));
}

TEST_F(PlatformNotificationServiceTest_WebApps, CreateNotificationFromData) {
  PlatformNotificationData notification_data;
  notification_data.title = u"My Notification";
  notification_data.body = u"Hello, world!";

  GURL origin = url::Origin::Create(kWebAppStartUrl).GetURL();

  Notification notification = service()->CreateNotificationFromData(
      origin, "id", notification_data, NotificationResources(),
      /*web_app_hint_url=*/kWebAppStartUrl);
  EXPECT_EQ(notification.notifier_id().web_app_id, installed_app_id);

  Notification nested_notification = service()->CreateNotificationFromData(
      origin, "id", notification_data, NotificationResources(),
      /*web_app_hint_url=*/kInstalledNestedWebAppStartUrl);
  EXPECT_EQ(nested_notification.notifier_id().web_app_id,
            nested_installed_app_id);
}

TEST_F(PlatformNotificationServiceTest_WebApps, PopulateWebAppId_MatchesScope) {
  service()->DisplayNotification(kNotificationId, kWebAppOrigin,
                                 kNotInstalledNestedWebAppStartUrl,
                                 kNotificationData, NotificationResources());
  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));
  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_NON_PERSISTENT);
  EXPECT_EQ(installed_app_id, notification.notifier_id().web_app_id);

  service()->DisplayPersistentNotification(kNotificationId, kServiceWorkerScope,
                                           kWebAppOrigin, kNotificationData,
                                           NotificationResources());
  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));
  notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_PERSISTENT);
  EXPECT_EQ(installed_app_id, notification.notifier_id().web_app_id);
}

TEST_F(PlatformNotificationServiceTest_WebApps,
       PopulateWebAppId_InNestedScope) {
  service()->DisplayNotification(kNotificationId, kWebAppOrigin,
                                 kInstalledNestedWebAppStartUrl,
                                 kNotificationData, NotificationResources());
  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));
  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_NON_PERSISTENT);
  EXPECT_EQ(nested_installed_app_id, notification.notifier_id().web_app_id);
}

TEST_F(PlatformNotificationServiceTest_WebApps, PopulateWebAppId_NotInScope) {
  service()->DisplayNotification(kNotificationId, kWebAppOrigin, kOutOfScopeUrl,
                                 kNotificationData, NotificationResources());
  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));
  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_NON_PERSISTENT);
  EXPECT_EQ(std::nullopt, notification.notifier_id().web_app_id);

  service()->DisplayPersistentNotification(kNotificationId, kOutOfScopeUrl,
                                           kWebAppOrigin, kNotificationData,
                                           NotificationResources());
  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));
  notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_PERSISTENT);
  EXPECT_EQ(std::nullopt, notification.notifier_id().web_app_id);
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(PlatformNotificationServiceTest, DisplayNameForContextMessage) {
  std::u16string display_name =
      service()->DisplayNameForContextMessage(GURL("https://chrome.com/"));

  EXPECT_TRUE(display_name.empty());

  // Create a mocked extension.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetID("honijodknafkokifofgiaalefdiedpko")
          .SetManifest(base::Value::Dict()
                           .Set("name", "NotificationTest")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Test Extension"))
          .Build();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_.get());
  EXPECT_TRUE(registry->AddEnabled(extension));

  display_name = service()->DisplayNameForContextMessage(
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"));
  EXPECT_EQ("NotificationTest", base::UTF16ToUTF8(display_name));
}

TEST_F(PlatformNotificationServiceTest, CreateNotificationFromData) {
  PlatformNotificationData notification_data;
  notification_data.title = u"My Notification";
  notification_data.body = u"Hello, world!";
  GURL origin("https://chrome.com/");

  Notification notification = service()->CreateNotificationFromData(
      origin, "id", notification_data, NotificationResources(),
      /*web_app_hint_url=*/GURL());
  EXPECT_TRUE(notification.context_message().empty());

  // Create a mocked extension.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetID("honijodknafkokifofgiaalefdiedpko")
          .SetManifest(base::Value::Dict()
                           .Set("name", "NotificationTest")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Test Extension"))
          .Build();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_.get());
  EXPECT_TRUE(registry->AddEnabled(extension));

  notification = service()->CreateNotificationFromData(
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"),
      "id", notification_data, NotificationResources(),
      /*web_app_hint_url=*/GURL());
  EXPECT_EQ("NotificationTest",
            base::UTF16ToUTF8(notification.context_message()));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS)
using PlatformNotificationServiceTest_WebAppNotificationIconAndTitle =
    PlatformNotificationServiceTest;

TEST_F(PlatformNotificationServiceTest_WebAppNotificationIconAndTitle,
       FindWebAppIconAndTitle_NoApp) {
  web_app::FakeWebAppProvider* provider =
      web_app::FakeWebAppProvider::Get(profile_.get());
  provider->Start();

  const GURL web_app_url{"https://example.org/"};
  EXPECT_FALSE(service()->FindWebAppIconAndTitle(web_app_url).has_value());
}

TEST_F(PlatformNotificationServiceTest_WebAppNotificationIconAndTitle,
       FindWebAppIconAndTitle) {
  web_app::FakeWebAppProvider* provider =
      web_app::FakeWebAppProvider::Get(profile_.get());
  web_app::WebAppIconManager& icon_manager = provider->GetIconManager();

  std::unique_ptr<web_app::WebApp> web_app = web_app::test::CreateWebApp();
  const GURL web_app_url = web_app->start_url();
  const webapps::AppId app_id = web_app->app_id();
  web_app->SetName("Web App Title");

  IconManagerWriteGeneratedIcons(icon_manager, app_id,
                                 {{web_app::IconPurpose::MONOCHROME,
                                   {web_app::icon_size::k16},
                                   {SK_ColorTRANSPARENT}}});
  web_app->SetDownloadedIconSizes(web_app::IconPurpose::MONOCHROME,
                                  {web_app::icon_size::k16});

  provider->GetRegistrarMutable().registry().emplace(app_id,
                                                     std::move(web_app));

  base::RunLoop run_loop;
  icon_manager.SetFaviconMonochromeReadCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const webapps::AppId& cached_app_id) { run_loop.Quit(); }));
  icon_manager.Start();
  run_loop.Run();

  provider->Start();

  std::optional<PlatformNotificationServiceImpl::WebAppIconAndTitle>
      icon_and_title = service()->FindWebAppIconAndTitle(web_app_url);

  ASSERT_TRUE(icon_and_title.has_value());
  EXPECT_EQ(u"Web App Title", icon_and_title->title);
  EXPECT_FALSE(icon_and_title->icon.isNull());
  EXPECT_EQ(
      SK_ColorTRANSPARENT,
      icon_and_title->icon.GetRepresentation(1.0f).GetBitmap().getColor(0, 0));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class PlatformNotificationServiceTest_NotificationContentDetection
    : public PlatformNotificationServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    if (IsSafeBrowsingEnabled()) {
      profile_->GetTestingPrefService()->SetManagedPref(
          prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
    } else {
      profile_->GetTestingPrefService()->SetManagedPref(
          prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(false));
    }
    mock_notification_content_detection_service_ = static_cast<
        safe_browsing::MockNotificationContentDetectionService*>(
        safe_browsing::NotificationContentDetectionServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating(
                    &safe_browsing::MockNotificationContentDetectionService::
                        FactoryForTests,
                    &model_observer_tracker_,
                    base::ThreadPool::CreateSequencedTaskRunner(
                        {base::MayBlock()}))));
  }

  void TearDown() override {
    mock_notification_content_detection_service_ = nullptr;
    PlatformNotificationServiceTest::TearDown();
  }

  bool IsSafeBrowsingEnabled() { return GetParam(); }

 protected:
  raw_ptr<safe_browsing::MockNotificationContentDetectionService>
      mock_notification_content_detection_service_ = nullptr;
  safe_browsing::TestModelObserverTracker model_observer_tracker_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PlatformNotificationServiceTest_NotificationContentDetection,
    testing::Bool());

TEST_P(PlatformNotificationServiceTest_NotificationContentDetection,
       PerformNotificationContentDetectionWhenEnabled) {
  PlatformNotificationData data;
  data.title = u"My notification's title";
  data.body = u"Hello, world!";

  int expected_number_of_calls = 0;
  if (IsSafeBrowsingEnabled()) {
    expected_number_of_calls = 1;
  }
  EXPECT_CALL(*mock_notification_content_detection_service_,
              MaybeCheckNotificationContentDetectionModel(_, _, _, _))
      .Times(expected_number_of_calls);
  service()->DisplayPersistentNotification(
      kNotificationId, GURL() /* service_worker_scope */,
      GURL("https://chrome.com/"), data, NotificationResources());
}

class PlatformNotificationServiceTest_ReportNotificationContentDetectionData
    : public PlatformNotificationServiceTest {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    scoped_feature_list_.InitWithFeatures(
        {safe_browsing::kReportNotificationContentDetectionData}, {});
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
    mock_notification_content_detection_service_ = static_cast<
        safe_browsing::MockNotificationContentDetectionService*>(
        safe_browsing::NotificationContentDetectionServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating(
                    &safe_browsing::MockNotificationContentDetectionService::
                        FactoryForTests,
                    &model_observer_tracker_,
                    base::ThreadPool::CreateSequencedTaskRunner(
                        {base::MayBlock()}))));
  }

  void TearDown() override {
    mock_notification_content_detection_service_ = nullptr;
    PlatformNotificationServiceTest::TearDown();
  }

 protected:
  raw_ptr<safe_browsing::MockNotificationContentDetectionService>
      mock_notification_content_detection_service_ = nullptr;
  safe_browsing::TestModelObserverTracker model_observer_tracker_;
};

TEST_F(PlatformNotificationServiceTest_ReportNotificationContentDetectionData,
       UpdateNotificationDatabaseMetadata) {
  // Store notification data in `NotificationDatabase`.
  const int64_t kFakeServiceWorkerRegistrationId = 42;
  int notification_id = 1;
  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;
  notification_database_data.origin = origin;
  GetPlatformNotificationContext(origin)->WriteNotificationData(
      notification_id, kFakeServiceWorkerRegistrationId, origin,
      notification_database_data, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  // Update `NotificationDatabase` entry with metadata.
  std::string full_notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);
  Notification notification = message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, full_notification_id_str,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  bool is_on_global_cache_list = false;
  bool is_allowlisted_by_user = false;
  double suspicious_score = 70.0;
  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification, std::move(metadata), /*should_show_warning=*/true,
      safe_browsing::NotificationContentDetectionModel::GetSerializedMetadata(
          is_on_global_cache_list, is_allowlisted_by_user, suspicious_score));
  // Read and check the entry from `NotificationDatabase`.
  NotificationDatabaseData data =
      ReadNotificationDataAndRecordInteractionSynchronous(
          GetPlatformNotificationContext(origin),
          base::NumberToString(notification_id), origin);
  ASSERT_TRUE(data.serialized_metadata.contains(
      safe_browsing::kNotificationContentDetectionMetadataDictionaryKey));
  ASSERT_EQ(
      safe_browsing::NotificationContentDetectionModel::GetSerializedMetadata(
          is_on_global_cache_list, is_allowlisted_by_user, suspicious_score),
      data.serialized_metadata.at(
          safe_browsing::kNotificationContentDetectionMetadataDictionaryKey));
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  base::Value cur_value(hcsm->GetWebsiteSetting(
      origin, origin, ContentSettingsType::SUSPICIOUS_NOTIFICATION_IDS));
#if BUILDFLAG(IS_ANDROID)
  const base::Value::List* suspicious_notification_ids =
      cur_value.GetDict().FindList(
          safe_browsing::kSuspiciousNotificationIdsKey);
  ASSERT_EQ(1u, suspicious_notification_ids->size());
  ASSERT_EQ(full_notification_id_str,
            suspicious_notification_ids->front().GetString());
#else
  ASSERT_TRUE(cur_value.is_none());
#endif
}

class PlatformNotificationServiceTest_AutoRevokeSuspiciousNotification
    : public PlatformNotificationServiceTest_ReportNotificationContentDetectionData {
 public:
  void SetUp() override {
    PlatformNotificationServiceTest_ReportNotificationContentDetectionData::
        SetUp();
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{safe_browsing::kAutoRevokeSuspiciousNotification,
          {{safe_browsing::
                kAutoRevokeSuspiciousNotificationEngagementScoreCutOff.name,
            "50.0"},
           {safe_browsing::kAutoRevokeSuspiciousNotificationMinNotificationCount
                .name,
            "2"},
           {safe_browsing::kAutoRevokeSuspiciousNotificationLookBackPeriod.name,
            "1"}}},
         {safe_browsing::kReportNotificationContentDetectionData, {}}},
        /*disabled_features=*/{});
  }

  void SetUpSuspiciousSite(const GURL& url, int suspicious_warning_count = 3) {
    SetNotificationPermission(url, ContentSetting::CONTENT_SETTING_ALLOW);
    SetSiteEngagementScore(url, 40.0);
    RecordSuspiciousNotifications(url, suspicious_warning_count);
  }

 private:
  void RecordSuspiciousNotifications(const GURL& url, int count) {
    HostContentSettingsMap* hcsm =
        HostContentSettingsMapFactory::GetForProfile(profile_.get());
    base::Value::Dict engagement;
    std::string date =
        permissions::NotificationsEngagementService::GetBucketLabel(
            base::Time::Now());
    base::Value::Dict* bucket =
        &engagement.Set(date, base::Value::Dict())->GetDict();
    bucket->Set("suspicious_count", count);
    hcsm->SetWebsiteSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS,
        base::Value(std::move(engagement)));
    base::Value stored_value = hcsm->GetWebsiteSetting(
        url, url, ContentSettingsType::NOTIFICATION_INTERACTIONS);
    EXPECT_EQ(count, permissions::NotificationsEngagementService::
                         GetSuspiciousNotificationCountForPeriod(
                             stored_value.GetDict(), 1));
  }

  void SetSiteEngagementScore(const GURL& url, double score) {
    site_engagement::SiteEngagementServiceFactory::GetForProfile(profile_.get())
        ->ResetBaseScoreForURL(url, score);
  }

  void SetNotificationPermission(const GURL& url, ContentSetting cs) {
    HostContentSettingsMap* hcsm =
        HostContentSettingsMapFactory::GetForProfile(profile_.get());
    hcsm->SetContentSettingDefaultScope(url, GURL(),
                                        ContentSettingsType::NOTIFICATIONS, cs);
  }
};

TEST_F(PlatformNotificationServiceTest_AutoRevokeSuspiciousNotification,
       RecordSuspiciousNotification) {
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  int notification_id = 1;
  GURL origin("https://example.com");
  std::string full_notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);
  Notification notification = message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, full_notification_id_str,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();

  std::string bucket_label =
      permissions::NotificationsEngagementService::GetBucketLabel(
          base::Time::Now());
  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification, std::move(metadata), /*should_show_warning=*/true,
      /* serialized_content_detection_metadata*/ std::nullopt);

  base::Value::Dict notification_engagement_dict =
      hcsm->GetWebsiteSetting(origin, GURL(),
                              ContentSettingsType::NOTIFICATION_INTERACTIONS)
          .GetDict()
          .Clone();
  ASSERT_EQ(1U, notification_engagement_dict.size());
  base::Value* daily_entry = notification_engagement_dict.Find(bucket_label);
  ASSERT_TRUE(daily_entry->is_dict());
  ASSERT_EQ(1, daily_entry->GetDict().FindInt("suspicious_count").value_or(0));
}

TEST_F(PlatformNotificationServiceTest_AutoRevokeSuspiciousNotification,
       NotSuspiciousNoEngagementRecorded) {
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  int notification_id = 1;
  GURL origin("https://example.com");
  std::string full_notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);
  Notification notification = message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, full_notification_id_str,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();

  std::string bucket_label =
      permissions::NotificationsEngagementService::GetBucketLabel(
          base::Time::Now());
  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification, std::move(metadata), /*should_show_warning=*/false,
      /* serialized_content_detection_metadata*/ std::nullopt);

  ContentSettingsForOneType notifications_engagement_setting =
      hcsm->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_EQ(0U, notifications_engagement_setting.size());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PlatformNotificationServiceTest_AutoRevokeSuspiciousNotification,
       RevokeNotificationPermission) {
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  // Set up notification.
  int notification_id = 1;
  GURL origin("https://example.com");
  std::string full_notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);
  Notification notification = message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, full_notification_id_str,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();

  // Simulate suspicious site to trigger auto-revocation.
  SetUpSuspiciousSite(origin, /*suspicious_warning_count*/ 1);

  // Notification should not be revoked prior to meeting revocation threshold.
  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification, std::move(metadata), /*should_show_warning=*/true,
      /* serialized_content_detection_metadata*/ std::nullopt);
  EXPECT_FALSE(safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm, origin));

  // Verify notification permission has been revoked.
  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification, std::move(metadata), /*should_show_warning=*/true,
      /* serialized_content_detection_metadata*/ std::nullopt);
  EXPECT_TRUE(safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm, origin));
  EXPECT_EQ(safe_browsing::NotificationRevocationSource::
                kSuspiciousContentAutoRevocation,
            AbusiveNotificationPermissionsManager::
                GetRevokedAbusiveNotificationRevocationSource(hcsm, origin));
}

TEST_F(PlatformNotificationServiceTest_AutoRevokeSuspiciousNotification,
       DoNotRevokeWhenFeatureDisabled) {
  scoped_feature_list_.Reset();

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  // Set up notification.
  int notification_id = 1;
  GURL origin("https://example.com");
  std::string full_notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);
  Notification notification = message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, full_notification_id_str,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();

  // Simulate suspicious site to trigger auto-revocation.
  SetUpSuspiciousSite(origin, /*suspicious_warning_count*/ 3);

  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification, std::move(metadata), /*should_show_warning=*/true,
      /* serialized_content_detection_metadata*/ std::nullopt);

  // Verify notification permission has not been revoked.
  EXPECT_FALSE(safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm, origin));
}

TEST_F(PlatformNotificationServiceTest_AutoRevokeSuspiciousNotification,
       RevokeNotificationPermission_UpdateNotificationDatabaseMetadata) {
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_.get());
  // Store notification data in `NotificationDatabase`.
  const int64_t kFakeServiceWorkerRegistrationId = 42;
  int notification_id_1 = 1;
  int notification_id_2 = 2;
  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;
  notification_database_data.origin = origin;
  GetPlatformNotificationContext(origin)->WriteNotificationData(
      notification_id_1, kFakeServiceWorkerRegistrationId, origin,
      notification_database_data, base::DoNothing());
  GetPlatformNotificationContext(origin)->WriteNotificationData(
      notification_id_2, kFakeServiceWorkerRegistrationId, origin,
      notification_database_data, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  // Update `NotificationDatabase` entry with metadata.
  std::string full_notification_1_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id_1);
  std::string full_notification_2_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id_2);
  Notification notification_1 = message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, full_notification_1_str,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  Notification notification_2 = message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, full_notification_2_str,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), origin, message_center::NotifierId(),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  bool is_on_global_cache_list = false;
  bool is_allowlisted_by_user = false;
  double suspicious_score = 70.0;

  // Simulate suspicious site to trigger auto-revocation.
  SetUpSuspiciousSite(origin, /*suspicious_warning_count*/ 1);

  // Notification should not be revoked prior to meeting revocation threshold.
  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification_1, std::make_unique<PersistentNotificationMetadata>(),
      /*should_show_warning=*/true,
      safe_browsing::NotificationContentDetectionModel::GetSerializedMetadata(
          is_on_global_cache_list, is_allowlisted_by_user, suspicious_score));
  EXPECT_FALSE(safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm, origin));
  // Notification metadata is stored as normal.
  NotificationDatabaseData data =
      ReadNotificationDataAndRecordInteractionSynchronous(
          GetPlatformNotificationContext(origin),
          base::NumberToString(notification_id_1), origin);
  EXPECT_TRUE(data.serialized_metadata.contains(
      safe_browsing::kNotificationContentDetectionMetadataDictionaryKey));
  EXPECT_EQ(
      safe_browsing::NotificationContentDetectionModel::GetSerializedMetadata(
          is_on_global_cache_list, is_allowlisted_by_user, suspicious_score),
      data.serialized_metadata.at(
          safe_browsing::kNotificationContentDetectionMetadataDictionaryKey));

  // Revoke notification permission.
  service()->HandleOnDeviceModelResponseThenMaybeDisplay(
      notification_2, std::make_unique<PersistentNotificationMetadata>(),
      /*should_show_warning=*/true,
      safe_browsing::NotificationContentDetectionModel::GetSerializedMetadata(
          is_on_global_cache_list, is_allowlisted_by_user, suspicious_score));
  // Verify notification permission has been revoked.
  EXPECT_TRUE(safety_hub_util::IsUrlRevokedAbusiveNotification(hcsm, origin));
  EXPECT_EQ(safe_browsing::NotificationRevocationSource::
                kSuspiciousContentAutoRevocation,
            AbusiveNotificationPermissionsManager::
                GetRevokedAbusiveNotificationRevocationSource(hcsm, origin));
  // Read and check the entry from `NotificationDatabase`.
  // Notification metadata should still be stored.
  NotificationDatabaseData post_revoked_data =
      ReadNotificationDataAndRecordInteractionSynchronous(
          GetPlatformNotificationContext(origin),
          base::NumberToString(notification_id_2), origin);
  ASSERT_TRUE(post_revoked_data.serialized_metadata.contains(
      safe_browsing::kNotificationContentDetectionMetadataDictionaryKey));
  ASSERT_EQ(
      safe_browsing::NotificationContentDetectionModel::GetSerializedMetadata(
          is_on_global_cache_list, is_allowlisted_by_user, suspicious_score),
      post_revoked_data.serialized_metadata.at(
          safe_browsing::kNotificationContentDetectionMetadataDictionaryKey));
}
#endif
