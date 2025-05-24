// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/metrics/mock_notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/notification_content_detection/mock_notification_content_detection_service.h"
#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"

using ::testing::_;
using ::testing::Return;
using PermissionStatus = blink::mojom::PermissionStatus;

MATCHER_P(PermissionTypeMatcher, id, "") {
  return ::testing::Matches(::testing::Eq(id))(
      blink::PermissionDescriptorToPermissionType(arg));
}
namespace {

const char kExampleNotificationId[] = "example_notification_id";
const char kExampleOrigin[] = "https://example.com";

class TestingProfileWithPermissionManager : public TestingProfile {
 public:
  TestingProfileWithPermissionManager()
      : permission_manager_(
            std::make_unique<
                testing::NiceMock<content::MockPermissionManager>>()) {}
  TestingProfileWithPermissionManager(
      const TestingProfileWithPermissionManager&) = delete;
  TestingProfileWithPermissionManager& operator=(
      const TestingProfileWithPermissionManager&) = delete;

  ~TestingProfileWithPermissionManager() override = default;

  // Sets the notification permission status to |permission_status|.
  void SetNotificationPermissionStatus(PermissionStatus permission_status) {
    ON_CALL(
        *permission_manager_,
        GetPermissionResultForOriginWithoutContext(
            PermissionTypeMatcher(blink::PermissionType::NOTIFICATIONS), _, _))
        .WillByDefault(Return(content::PermissionResult(
            permission_status, content::PermissionStatusSource::UNSPECIFIED)));
  }

  // TestingProfile overrides:
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return permission_manager_.get();
  }

 private:
  std::unique_ptr<content::MockPermissionManager> permission_manager_;
};

}  // namespace

class PersistentNotificationHandlerTest : public ::testing::Test {
 public:
  PersistentNotificationHandlerTest()
      : profile_(std::make_unique<TestingProfileWithPermissionManager>()),
        display_service_tester_(profile_.get()),
        origin_(kExampleOrigin) {}
  PersistentNotificationHandlerTest(const PersistentNotificationHandlerTest&) =
      delete;
  PersistentNotificationHandlerTest& operator=(
      const PersistentNotificationHandlerTest&) = delete;

  ~PersistentNotificationHandlerTest() override = default;

  // ::testing::Test overrides:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {safe_browsing::kOnDeviceNotificationContentDetectionModel,
             safe_browsing::kShowWarningsForSuspiciousNotifications});

    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), HistoryServiceFactory::GetDefaultFactory());

    mock_logger_ = static_cast<MockNotificationMetricsLogger*>(
        NotificationMetricsLoggerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating(
                    &MockNotificationMetricsLogger::FactoryForTests)));

    PlatformNotificationServiceFactory::GetForProfile(profile_.get())
        ->ClearClosedNotificationsForTesting();
  }

  void TearDown() override {
    mock_logger_ = nullptr;
    profile_.reset();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileWithPermissionManager> profile_;
  NotificationDisplayServiceTester display_service_tester_;

  // The origin for which these tests are being run.
  GURL origin_;

  // Owned by the |profile_| as a keyed service.
  raw_ptr<MockNotificationMetricsLogger> mock_logger_ = nullptr;
};

TEST_F(PersistentNotificationHandlerTest, OnClick_WithoutPermission) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClickWithoutPermission());
  profile_->SetNotificationPermissionStatus(PermissionStatus::DENIED);

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClick(profile_.get(), origin_, kExampleNotificationId,
                   std::nullopt /* action_index */, std::nullopt /* reply */,
                   base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest,
       OnClick_CloseUnactionableNotifications) {
  // Show a notification for a particular origin.
  {
    base::RunLoop run_loop;
    display_service_tester_.SetNotificationAddedClosure(run_loop.QuitClosure());

    EXPECT_CALL(*mock_logger_, LogPersistentNotificationShown());

    PlatformNotificationServiceFactory::GetForProfile(profile_.get())
        ->DisplayPersistentNotification(
            kExampleNotificationId, origin_ /* service_worker_scope */, origin_,
            blink::PlatformNotificationData(), blink::NotificationResources());

    run_loop.Run();
  }

  ASSERT_TRUE(display_service_tester_.GetNotification(kExampleNotificationId));

  // Revoke permission for any origin to display notifications.
  profile_->SetNotificationPermissionStatus(PermissionStatus::DENIED);

  // Now simulate a click on the notification. It should be automatically closed
  // by the PersistentNotificationHandler.
  {
    EXPECT_CALL(*mock_logger_,
                LogPersistentNotificationClickWithoutPermission());

    display_service_tester_.SimulateClick(
        NotificationHandler::Type::WEB_PERSISTENT, kExampleNotificationId,
        std::nullopt /* action_index */, std::nullopt /* reply */);
  }

  EXPECT_FALSE(display_service_tester_.GetNotification(kExampleNotificationId));
}

TEST_F(PersistentNotificationHandlerTest, OnClose_ByUser) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedByUser());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(profile_.get(), origin_, kExampleNotificationId,
                   /* by_user= */ true, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, OnClose_Programmatically) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedProgrammatically());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(profile_.get(), origin_, kExampleNotificationId,
                   /* by_user= */ false, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, DisableNotifications) {
  std::unique_ptr<NotificationPermissionContext> permission_context =
      std::make_unique<NotificationPermissionContext>(profile_.get());

  ASSERT_EQ(permission_context
                ->GetPermissionStatus(
                    content::PermissionDescriptorUtil::
                        CreatePermissionDescriptorForPermissionType(
                            blink::PermissionType::NOTIFICATIONS),
                    nullptr /* render_frame_host */, origin_, origin_)
                .status,
            PermissionStatus::ASK);

  // Set `ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER` to true for
  // `origin_`.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile_.get());
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(origin_),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
      base::Value(base::Value::Dict().Set(
          safe_browsing::kIsAllowlistedByUserKey, true)));

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->DisableNotifications(profile_.get(), origin_);

  // Disabling the permission should set
  // `ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER` to false.
  content_settings::SettingInfo info;
  base::Value value = hcsm->GetWebsiteSetting(
      origin_, origin_,
      ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
      &info);
  EXPECT_EQ(
      false,
      value.GetDict().FindBool(safe_browsing::kIsAllowlistedByUserKey).value());

#if BUILDFLAG(IS_ANDROID)
  PermissionStatus kExpectedDisabledStatus = PermissionStatus::ASK;
#else
  PermissionStatus kExpectedDisabledStatus = PermissionStatus::DENIED;
#endif
  ASSERT_EQ(permission_context
                ->GetPermissionStatus(
                    content::PermissionDescriptorUtil::
                        CreatePermissionDescriptorForPermissionType(
                            blink::PermissionType::NOTIFICATIONS),
                    nullptr /* render_frame_host */, origin_, origin_)
                .status,
            kExpectedDisabledStatus);
}

class PersistentNotificationHandlerWithNotificationContentDetection
    : public PersistentNotificationHandlerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    if (IsNotificationContentDetectionEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {safe_browsing::kOnDeviceNotificationContentDetectionModel},
          {safe_browsing::kShowWarningsForSuspiciousNotifications});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {safe_browsing::kOnDeviceNotificationContentDetectionModel,
               safe_browsing::kShowWarningsForSuspiciousNotifications});
    }
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
    PersistentNotificationHandlerTest::TearDown();
  }

  bool IsSafeBrowsingEnabled() { return std::get<0>(GetParam()); }

  bool IsNotificationContentDetectionEnabled() {
    return std::get<1>(GetParam());
  }

 protected:
  raw_ptr<safe_browsing::MockNotificationContentDetectionService>
      mock_notification_content_detection_service_ = nullptr;
  safe_browsing::TestModelObserverTracker model_observer_tracker_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PersistentNotificationHandlerWithNotificationContentDetection,
    testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(PersistentNotificationHandlerWithNotificationContentDetection,
       PerformNotificationContentDetectionWhenEnabled) {
  base::RunLoop run_loop;
  display_service_tester_.SetNotificationAddedClosure(run_loop.QuitClosure());

  int expected_number_of_calls = 0;
  if (IsSafeBrowsingEnabled() && IsNotificationContentDetectionEnabled()) {
    expected_number_of_calls = 1;
  }
  EXPECT_CALL(*mock_notification_content_detection_service_,
              MaybeCheckNotificationContentDetectionModel(_, _, _, _))
      .Times(expected_number_of_calls);

  PlatformNotificationServiceFactory::GetForProfile(profile_.get())
      ->DisplayPersistentNotification(
          kExampleNotificationId, origin_ /* service_worker_scope */, origin_,
          blink::PlatformNotificationData(), blink::NotificationResources());

  run_loop.Run();
}

class
    PersistentNotificationHandlerWithNotificationContentDetectionAndLoggingTest
    : public PersistentNotificationHandlerTest {
 public:
  PersistentNotificationHandlerWithNotificationContentDetectionAndLoggingTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());

    testing_profile_ = manager_.CreateTestingProfile("foo");
    mock_optimization_guide_keyed_service_ = static_cast<
        MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                testing_profile_,
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockOptimizationGuideKeyedService>();
                })));
    auto logs_uploader = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(
        manager_.local_state()->Get());
    mock_optimization_guide_keyed_service_
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::move(logs_uploader));
  }

  void TearDown() override { PersistentNotificationHandlerTest::TearDown(); }

  TestingProfile* profile() { return testing_profile_; }

  optimization_guide::TestModelQualityLogsUploaderService* logs_uploader() {
    return static_cast<
        optimization_guide::TestModelQualityLogsUploaderService*>(
        mock_optimization_guide_keyed_service_
            ->GetModelQualityLogsUploaderService());
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  uploaded_logs() {
    return logs_uploader()->uploaded_logs();
  }

  content::PlatformNotificationContext* GetPlatformNotificationContext(
      GURL origin) {
    return testing_profile_->GetStoragePartitionForUrl(origin)
        ->GetPlatformNotificationContext();
  }

  void WriteNotificationDataAndMetadataToDatabase(bool is_on_global_cache_list,
                                                  bool is_allowlisted_by_user,
                                                  double suspicious_score) {
    // Store notification data in `NotificationDatabase`.
    const int64_t kFakeServiceWorkerRegistrationId = 42;
    int notification_id = 1;
    GURL origin(kExampleOrigin);
    content::NotificationDatabaseData notification_database_data;
    notification_database_data.origin = origin;
    GetPlatformNotificationContext(origin)->WriteNotificationData(
        notification_id, kFakeServiceWorkerRegistrationId, origin,
        notification_database_data, base::DoNothing());
    base::RunLoop().RunUntilIdle();

    // Store metadata in `NotificationDatabase`.
    std::string notification_id_str =
        "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);
    std::string serialized_metadata =
        "{\"" +
        std::string(safe_browsing::kMetadataIsOriginAllowlistedByUserKey) +
        "\":" + (is_allowlisted_by_user ? "true" : "false") + ",\"" +
        std::string(safe_browsing::kMetadataIsOriginOnGlobalCacheListKey) +
        "\":" + (is_on_global_cache_list ? "true" : "false") + ",\"" +
        std::string(safe_browsing::kMetadataSuspiciousKey) +
        "\":" + base::NumberToString(suspicious_score) + "}";
    GetPlatformNotificationContext(origin)->WriteNotificationMetadata(
        notification_id_str, origin, safe_browsing::kMetadataDictionaryKey,
        serialized_metadata, base::DoNothing());
  }

 private:
  TestingProfileManager manager_;
  raw_ptr<TestingProfile> testing_profile_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
};

TEST_F(
    PersistentNotificationHandlerWithNotificationContentDetectionAndLoggingTest,
    ReportNotificationAsSafe) {
  bool is_url_on_allowlist = true;
  bool did_user_always_allow_url = false;
  double suspicious_score = 70.0;
  WriteNotificationDataAndMetadataToDatabase(
      is_url_on_allowlist, did_user_always_allow_url, suspicious_score);
  int notification_id = 1;

  GURL origin(origin_);
  std::string notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->ReportNotificationAsSafe(notification_id_str, origin_, profile());

  // Check the MQLS log.
  ASSERT_TRUE(log_uploaded_signal.Wait());
  const auto& logs = uploaded_logs();
  ASSERT_EQ(1u, logs.size());
  auto* const notification_content_detection =
      logs[0]->mutable_notification_content_detection();
  ASSERT_TRUE(notification_content_detection->has_request());
  ASSERT_TRUE(notification_content_detection->has_response());
  ASSERT_TRUE(notification_content_detection->has_quality());
  ASSERT_EQ(
      origin.spec(),
      notification_content_detection->request().notification_contents().url());
  ASSERT_EQ(suspicious_score,
            notification_content_detection->response().suspicious_score());
  ASSERT_TRUE(notification_content_detection->quality().is_url_on_allowlist());
  ASSERT_FALSE(
      notification_content_detection->quality().did_user_always_allow_url());
  ASSERT_TRUE(
      notification_content_detection->quality().was_user_shown_warning());
  ASSERT_FALSE(
      notification_content_detection->quality().did_user_unsubscribe());
  ASSERT_EQ(optimization_guide::proto::SiteEngagementScore::
                SITE_ENGAGEMENT_SCORE_NONE,
            notification_content_detection->quality().site_engagement_score());
}

TEST_F(
    PersistentNotificationHandlerWithNotificationContentDetectionAndLoggingTest,
    ReportWarnedNotificationAsSpam) {
  bool is_url_on_allowlist = true;
  bool did_user_always_allow_url = false;
  double suspicious_score = 70.0;
  WriteNotificationDataAndMetadataToDatabase(
      is_url_on_allowlist, did_user_always_allow_url, suspicious_score);
  int notification_id = 1;

  GURL origin(origin_);
  std::string notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->ReportWarnedNotificationAsSpam(notification_id_str, origin_,
                                          profile());

  // Check the MQLS log.
  ASSERT_TRUE(log_uploaded_signal.Wait());
  const auto& logs = uploaded_logs();
  ASSERT_EQ(1u, logs.size());
  auto* const notification_content_detection =
      logs[0]->mutable_notification_content_detection();
  ASSERT_TRUE(notification_content_detection->has_request());
  ASSERT_TRUE(notification_content_detection->has_response());
  ASSERT_TRUE(notification_content_detection->has_quality());
  ASSERT_EQ(
      origin.spec(),
      notification_content_detection->request().notification_contents().url());
  ASSERT_EQ(suspicious_score,
            notification_content_detection->response().suspicious_score());
  ASSERT_TRUE(notification_content_detection->quality().is_url_on_allowlist());
  ASSERT_FALSE(
      notification_content_detection->quality().did_user_always_allow_url());
  ASSERT_TRUE(
      notification_content_detection->quality().was_user_shown_warning());
  ASSERT_TRUE(notification_content_detection->quality().did_user_unsubscribe());
  ASSERT_EQ(optimization_guide::proto::SiteEngagementScore::
                SITE_ENGAGEMENT_SCORE_NONE,
            notification_content_detection->quality().site_engagement_score());
}

TEST_F(
    PersistentNotificationHandlerWithNotificationContentDetectionAndLoggingTest,
    ReportUnwarnedNotificationAsSpam) {
  bool is_url_on_allowlist = true;
  bool did_user_always_allow_url = false;
  double suspicious_score = 70.0;
  WriteNotificationDataAndMetadataToDatabase(
      is_url_on_allowlist, did_user_always_allow_url, suspicious_score);
  int notification_id = 1;

  GURL origin(origin_);
  std::string notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->ReportUnwarnedNotificationAsSpam(notification_id_str, origin_,
                                            profile());

  // Check the MQLS log.
  ASSERT_TRUE(log_uploaded_signal.Wait());
  const auto& logs = uploaded_logs();
  ASSERT_EQ(1u, logs.size());
  auto* const notification_content_detection =
      logs[0]->mutable_notification_content_detection();
  ASSERT_TRUE(notification_content_detection->has_request());
  ASSERT_TRUE(notification_content_detection->has_response());
  ASSERT_TRUE(notification_content_detection->has_quality());
  ASSERT_EQ(
      origin.spec(),
      notification_content_detection->request().notification_contents().url());
  ASSERT_EQ(suspicious_score,
            notification_content_detection->response().suspicious_score());
  ASSERT_TRUE(notification_content_detection->quality().is_url_on_allowlist());
  ASSERT_FALSE(
      notification_content_detection->quality().did_user_always_allow_url());
  ASSERT_FALSE(
      notification_content_detection->quality().was_user_shown_warning());
  ASSERT_TRUE(notification_content_detection->quality().did_user_unsubscribe());
  ASSERT_EQ(optimization_guide::proto::SiteEngagementScore::
                SITE_ENGAGEMENT_SCORE_NONE,
            notification_content_detection->quality().site_engagement_score());
}

class
    PersistentNotificationHandlerWithNotificationContentDetectionLowLoggingRateTest
    : public PersistentNotificationHandlerWithNotificationContentDetectionAndLoggingTest {
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        safe_browsing::kReportNotificationContentDetectionData,
        {{"ReportNotificationContentDetectionDataRate", "0"}});
    PersistentNotificationHandlerWithNotificationContentDetectionAndLoggingTest::
        SetUp();
  }
};

TEST_F(
    PersistentNotificationHandlerWithNotificationContentDetectionLowLoggingRateTest,
    NoReportSent) {
  bool is_url_on_allowlist = true;
  bool did_user_always_allow_url = false;
  double suspicious_score = 70.0;
  WriteNotificationDataAndMetadataToDatabase(
      is_url_on_allowlist, did_user_always_allow_url, suspicious_score);
  int notification_id = 1;

  GURL origin(origin_);
  std::string notification_id_str =
      "p#" + origin.spec() + "#0" + base::NumberToString(notification_id);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->ReportNotificationAsSafe(notification_id_str, origin_, profile());

  // Check no MQLS logs.
  const auto& logs = uploaded_logs();
  ASSERT_EQ(0u, logs.size());
}
