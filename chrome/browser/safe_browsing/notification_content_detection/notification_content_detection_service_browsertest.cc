// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"

#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/persistent_notification_handler.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"

namespace safe_browsing {

namespace {

constexpr char kAllowlistedUrl[] = "https://allowlisted.url/";
constexpr char kNonAllowlistedUrl[] = "https://non-allowlisted.url/";
constexpr char kNotificationId[] = "my-notification-id";

}  // namespace

class NotificationContentDetectionBrowserTestBase
    : public InProcessBrowserTest {
 public:
  NotificationContentDetectionBrowserTestBase() = default;

  NotificationContentDetectionBrowserTestBase(
      const NotificationContentDetectionBrowserTestBase&) = delete;
  NotificationContentDetectionBrowserTestBase& operator=(
      const NotificationContentDetectionBrowserTestBase&) = delete;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new TestSafeBrowsingUIManager());
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
  }

 protected:
  TestSafeBrowsingServiceFactory factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

using NotificationContentDetectionServiceFactoryBrowserTest =
    NotificationContentDetectionBrowserTestBase;

IN_PROC_BROWSER_TEST_F(NotificationContentDetectionServiceFactoryBrowserTest,
                       EnabledForRegularProfiles) {
  EXPECT_NE(nullptr, NotificationContentDetectionServiceFactory::GetForProfile(
                         browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(NotificationContentDetectionServiceFactoryBrowserTest,
                       DisabledForIncognitoMode) {
  auto test_profile = TestingProfile::Builder().Build();
  TestingProfile* incognito =
      TestingProfile::Builder().BuildIncognito(test_profile.get());
  EXPECT_EQ(nullptr, NotificationContentDetectionServiceFactory::GetForProfile(
                         incognito));
}

IN_PROC_BROWSER_TEST_F(NotificationContentDetectionServiceFactoryBrowserTest,
                       DisabledForGuestMode) {
  auto test_profile = TestingProfile::Builder().Build();
  TestingProfile* off_the_record = TestingProfile::Builder().BuildOffTheRecord(
      test_profile.get(), Profile::OTRProfileID::CreateUniqueForTesting());
  EXPECT_EQ(nullptr, NotificationContentDetectionServiceFactory::GetForProfile(
                         off_the_record));
}

class NotificationContentDetectionBrowserTest
    : public NotificationContentDetectionBrowserTestBase {
 public:
  NotificationContentDetectionBrowserTest() = default;
  NotificationContentDetectionBrowserTest(
      const NotificationContentDetectionBrowserTest&) = delete;
  NotificationContentDetectionBrowserTest& operator=(
      const NotificationContentDetectionBrowserTest&) = delete;

  void SetUp() override {
    // Disable the `kPreventLongRunningPredictionModels` feature to prevent
    // flaky test failures, since these tests may prompt models and obtaining
    // the result can take a long time.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            optimization_guide::features::kPreventLongRunningPredictionModels,
            safe_browsing::kShowWarningsForSuspiciousNotifications});
    NotificationContentDetectionBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    notification_content_detection_service_ =
        NotificationContentDetectionServiceFactory::GetForProfile(
            browser()->profile());
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());

    // Set up allowlisted and non-allowlisted URLs.
    SetNotificationsGlobalCacheListDomainsForTesting(
        {GURL(kAllowlistedUrl).GetHost()});
    SetURLHighConfidenceAllowlistMatch(GURL(kAllowlistedUrl),
                                       /*match_allowlist=*/true);
    SetURLHighConfidenceAllowlistMatch(GURL(kNonAllowlistedUrl),
                                       /*match_allowlist=*/false);
  }

  void TearDownOnMainThread() override {
    notification_content_detection_service_ = nullptr;
  }

  blink::PlatformNotificationData CreateNotificationData(
      std::u16string title,
      std::u16string body,
      std::vector<std::u16string> action_texts) {
    blink::PlatformNotificationData data;
    data.title = title;
    data.body = body;
    data.actions.resize(action_texts.size());
    for (size_t i = 0; i < action_texts.size(); ++i) {
      data.actions[i] = blink::mojom::NotificationAction::New();
      data.actions[i]->title = action_texts[i];
    }
    return data;
  }

  PlatformNotificationServiceImpl* service() {
    return PlatformNotificationServiceFactory::GetForProfile(
        browser()->profile());
  }

  NotificationContentDetectionService*
  notification_content_detection_service() {
    return notification_content_detection_service_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  std::vector<message_center::Notification>
  GetDisplayedPersistentNotifications() {
    return display_service_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::WEB_PERSISTENT);
  }

  bool IsNotificationSuspicious(
      const message_center::Notification& notification) {
    return PersistentNotificationMetadata::From(
               display_service_tester_->GetMetadataForNotification(
                   notification))
        ->is_suspicious;
  }

  void UpdateNotificationContentDetectionModel() {
    base::FilePath source_root_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir));
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("chrome")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("safe_browsing")
            .AppendASCII("notification_content_detection_bert_model.tflite");
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetModelForTesting(
            optimization_guide::proto::
                OPTIMIZATION_TARGET_NOTIFICATION_CONTENT_DETECTION,
            optimization_guide::TestModelInfoBuilder()
                .SetModelFilePath(model_file_path)
                .Build());
  }

 private:
  void SetURLHighConfidenceAllowlistMatch(const GURL& url,
                                          bool match_allowlist) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->SetHighConfidenceAllowlistMatchResult(url, match_allowlist);
  }

  raw_ptr<NotificationContentDetectionService>
      notification_content_detection_service_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

IN_PROC_BROWSER_TEST_F(NotificationContentDetectionBrowserTest,
                       NonAllowlistedSiteSuccessfullyExecutesModel) {
  UpdateNotificationContentDetectionModel();
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Non-allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL(kNonAllowlistedUrl) /* service_worker_scope */,
      GURL(kNonAllowlistedUrl), data, blink::NotificationResources());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      1);

  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      /*kSuccess=*/1, 1);
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 1);
  // The suspicious score histogram should be logged once, but the actual value
  // in this test is not important.
  histogram_tester().ExpectTotalCount(kSuspiciousScoreHistogram, 1);
  // Check that we are recording the UKM with the correct metric values.
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PermissionUsage_NotificationShown::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0],
                                      "DidUserAlwaysAllowNotifications", false);
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0], "IsAllowlisted", false);
  test_ukm_recorder.EntryHasMetric(ukm_entries[0], "SuspiciousScore");
  test_ukm_recorder.EntryHasMetric(ukm_entries[0], "SiteEngagementLevel");
}

class NotificationContentDetectionAllowlistedChecksEnabledBrowserTest
    : public NotificationContentDetectionBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NotificationContentDetectionAllowlistedChecksEnabledBrowserTest() = default;

  void SetUp() override {
    // Disable the `kPreventLongRunningPredictionModels` feature to prevent
    // flaky test failures, since these tests may prompt models and obtaining
    // the result can take a long time.
    if (IsGlobalCacheListFeatureEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {kGlobalCacheListForGatingNotificationProtections},
          /*disabled_features=*/{
              optimization_guide::features::kPreventLongRunningPredictionModels,
              safe_browsing::kShowWarningsForSuspiciousNotifications});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              optimization_guide::features::kPreventLongRunningPredictionModels,
              safe_browsing::kShowWarningsForSuspiciousNotifications,
              kGlobalCacheListForGatingNotificationProtections});
    }
    NotificationContentDetectionBrowserTestBase::SetUp();
  }

  bool IsGlobalCacheListFeatureEnabled() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    NotificationContentDetectionAllowlistedChecksEnabledBrowserTest,
    testing::Bool());

IN_PROC_BROWSER_TEST_P(
    NotificationContentDetectionAllowlistedChecksEnabledBrowserTest,
    AllowlistedSiteExecutesModel) {
  UpdateNotificationContentDetectionModel();
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL(kAllowlistedUrl) /* service_worker_scope */,
      GURL(kAllowlistedUrl), data, blink::NotificationResources());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      1);

  histogram_tester().ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      /*kSuccess=*/1, 1);
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 1);

  // The suspicious score histogram should be logged once, but the actual value
  // in this test is not important.
  histogram_tester().ExpectTotalCount(kSuspiciousScoreHistogram, 1);
  // Check that we are recording the UKM with the correct metric values.
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PermissionUsage_NotificationShown::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0],
                                      "DidUserAlwaysAllowNotifications", false);
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0], "IsAllowlisted", true);
  test_ukm_recorder.EntryHasMetric(ukm_entries[0], "SuspiciousScore");
  test_ukm_recorder.EntryHasMetric(ukm_entries[0], "SiteEngagementLevel");

  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  EXPECT_FALSE(
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
}

IN_PROC_BROWSER_TEST_P(
    NotificationContentDetectionAllowlistedChecksEnabledBrowserTest,
    NotificationDisplayedWhenModelNotAvailable) {
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL(kAllowlistedUrl) /* service_worker_scope */,
      GURL("https://chrome.com/"), data, blink::NotificationResources());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      0);

  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  EXPECT_FALSE(
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
}

class NotificationContentDetectionShowWarningsEnabledBrowserTest
    : public NotificationContentDetectionBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::string, std::string, bool>> {
 public:
  NotificationContentDetectionShowWarningsEnabledBrowserTest() = default;

  void SetUp() override {
    // Disable the `kPreventLongRunningPredictionModels` feature to prevent
    // flaky test failures, since these tests may prompt models and obtaining
    // the result can take a long time.
    if (IsGlobalCacheListFeatureEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{safe_browsing::kShowWarningsForSuspiciousNotifications,
            {{"ShowWarningsForSuspiciousNotificationsScoreThreshold",
              GetSuspiciousNotificationThreshold()}}},
           {kGlobalCacheListForGatingNotificationProtections, {}}},
          /*disabled_features=*/{optimization_guide::features::
                                     kPreventLongRunningPredictionModels});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{safe_browsing::kShowWarningsForSuspiciousNotifications,
            {{"ShowWarningsForSuspiciousNotificationsScoreThreshold",
              GetSuspiciousNotificationThreshold()}}}},
          /*disabled_features=*/{
              optimization_guide::features::kPreventLongRunningPredictionModels,
              kGlobalCacheListForGatingNotificationProtections});
    }

    NotificationContentDetectionBrowserTestBase::SetUp();
  }

  std::string GetAllowlistSamplingRate() { return std::get<0>(GetParam()); }
  std::string GetSuspiciousNotificationThreshold() {
    return std::get<1>(GetParam());
  }
  bool IsGlobalCacheListFeatureEnabled() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    NotificationContentDetectionShowWarningsEnabledBrowserTest,
    testing::Values(std::make_tuple("0", "0", true),
                    std::make_tuple("0", "0", false),
                    std::make_tuple("100", "0", true),
                    std::make_tuple("100", "0", false),
                    std::make_tuple("100", "100", true),
                    std::make_tuple("100", "100", false)));

// TODO(crbug.com/389745686): Re-enable
IN_PROC_BROWSER_TEST_P(
    NotificationContentDetectionShowWarningsEnabledBrowserTest,
    DISABLED_NonAllowlistedSiteNotificationGetsDisplayed) {
  UpdateNotificationContentDetectionModel();
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Non-allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL() /* service_worker_scope */,
      GURL(kNonAllowlistedUrl), data, blink::NotificationResources());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      1);

  ASSERT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  // When the suspicious threshold is 0, every non-allowlisted notification is
  // suspicious.
  if (GetSuspiciousNotificationThreshold() == "0") {
    EXPECT_TRUE(
        IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
  } else {
    EXPECT_FALSE(
        IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
  }
  histogram_tester().ExpectTotalCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      2);
  histogram_tester().ExpectBucketCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      /*kRequested=*/0, 1);
  histogram_tester().ExpectBucketCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      /*kFinished=*/1, 1);
}

IN_PROC_BROWSER_TEST_P(
    NotificationContentDetectionShowWarningsEnabledBrowserTest,
    AllowlistedSiteNotificationGetsDisplayed) {
  UpdateNotificationContentDetectionModel();
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL() /* service_worker_scope */, GURL(kAllowlistedUrl),
      data, blink::NotificationResources());

  if (GetAllowlistSamplingRate() == "100") {
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester(),
        "OptimizationGuide.ModelExecutor.ExecutionStatus."
        "NotificationContentDetection",
        1);
  } else {
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester(),
        "OptimizationGuide.ModelExecutor.ExecutionStatus."
        "NotificationContentDetection",
        0);
  }

  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  // When the model is checked for all allowlisted sites and the suspicious
  // threshold is 0, the notification is not suspicious for allowlisted URLs.
  EXPECT_FALSE(
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
  histogram_tester().ExpectTotalCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      2);
  histogram_tester().ExpectBucketCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      /*kRequested=*/0, 1);
  histogram_tester().ExpectBucketCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      /*kFinished=*/1, 1);
}

IN_PROC_BROWSER_TEST_P(
    NotificationContentDetectionShowWarningsEnabledBrowserTest,
    NotificationDisplayedWhenModelNotAvailable) {
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL(kAllowlistedUrl) /* service_worker_scope */,
      GURL("https://chrome.com/"), data, blink::NotificationResources());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      0);

  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  EXPECT_FALSE(
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
  histogram_tester().ExpectTotalCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      2);
  histogram_tester().ExpectBucketCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      /*kRequested=*/0, 1);
  histogram_tester().ExpectBucketCount(
      "SafeBrowsing.NotificationContentDetection."
      "DisplayPersistentNotificationEvent",
      /*kFinished=*/1, 1);
}

IN_PROC_BROWSER_TEST_P(
    NotificationContentDetectionShowWarningsEnabledBrowserTest,
    OriginalNotificationDisplayedWhenUserAllowlistsOrigin) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Set `ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER` to true for
  // `kNonAllowlistedUrl`.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetWebsiteSettingCustomScope(
          ContentSettingsPattern::FromURLNoWildcard(GURL(kNonAllowlistedUrl)),
          ContentSettingsPattern::Wildcard(),
          ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
          base::Value(base::Value::Dict().Set(kIsAllowlistedByUserKey, true)));

  UpdateNotificationContentDetectionModel();
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Non-allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL() /* service_worker_scope */,
      GURL(kNonAllowlistedUrl), data, blink::NotificationResources());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      1);

  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  // Notification should never be considered suspicious when the user has
  // allowlisted the origin.
  EXPECT_FALSE(
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
  // Check that we are recording the UKM with the correct metric values.
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PermissionUsage_NotificationShown::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0],
                                      "DidUserAlwaysAllowNotifications", true);
  test_ukm_recorder.ExpectEntryMetric(ukm_entries[0], "IsAllowlisted", false);
  test_ukm_recorder.EntryHasMetric(ukm_entries[0], "SuspiciousScore");
  test_ukm_recorder.EntryHasMetric(ukm_entries[0], "SiteEngagementLevel");
}

IN_PROC_BROWSER_TEST_P(
    NotificationContentDetectionShowWarningsEnabledBrowserTest,
    WarningShownWhenUserAllowlistsOriginThenUnsubscribesThenResubscribes) {
  // Set `ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER` to true for
  // `kNonAllowlistedUrl`.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetWebsiteSettingCustomScope(
          ContentSettingsPattern::FromURLNoWildcard(GURL(kNonAllowlistedUrl)),
          ContentSettingsPattern::Wildcard(),
          ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
          base::Value(base::Value::Dict().Set(kIsAllowlistedByUserKey, true)));

  // Unsubscribe from notifications then re-enable them.
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->DisableNotifications(browser()->profile(), GURL(kNonAllowlistedUrl),
                                /*notification_id=*/std::nullopt,
                                /*is_suspicious=*/false);
  NotificationPermissionContext::UpdatePermission(
      browser()->profile(), GURL(kNonAllowlistedUrl), CONTENT_SETTING_ALLOW);

  // Display persistent notification.
  UpdateNotificationContentDetectionModel();
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Non-allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId, GURL() /* service_worker_scope */,
      GURL(kNonAllowlistedUrl), data, blink::NotificationResources());
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      1);
  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);

  // When the suspicious threshold is 0, every non-allowlisted notification is
  // suspicious.
  if (GetSuspiciousNotificationThreshold() == "0") {
    EXPECT_TRUE(
        IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
  } else {
    EXPECT_FALSE(
        IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]));
  }
}

IN_PROC_BROWSER_TEST_F(NotificationContentDetectionBrowserTest,
                       EnterpriseAllowlistedSiteDoesNotExecuteModel) {
  // Setup enterprise allowlist.
  base::Value::List allowlist;
  allowlist.Append("enterprise-domain.com");
  browser()->profile()->GetPrefs()->SetList(
      prefs::kSafeBrowsingAllowlistDomains, std::move(allowlist));

  UpdateNotificationContentDetectionModel();
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  blink::PlatformNotificationData data =
      CreateNotificationData(u"Non-allowlisted title", u"Hello, world!", {});
  service()->DisplayPersistentNotification(
      kNotificationId,
      GURL("https://www.enterprise-domain.com") /* service_worker_scope */,
      GURL("https://www.enterprise-domain.com"), data,
      blink::NotificationResources());

  // No logging should have occurred, but the notification should have been
  // displayed.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester(),
      "OptimizationGuide.ModelExecutor.ExecutionStatus."
      "NotificationContentDetection",
      0);
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 0);
  histogram_tester().ExpectTotalCount(kSuspiciousScoreHistogram, 0);
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PermissionUsage_NotificationShown::kEntryName);
  EXPECT_EQ(0u, ukm_entries.size());
  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
}

class NotificationContentDetectionLoggingEnabledBrowserTest
    : public NotificationContentDetectionBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NotificationContentDetectionLoggingEnabledBrowserTest() = default;

  void SetUp() override {
    // Disable the `kPreventLongRunningPredictionModels` feature to prevent
    // flaky test failures, since these tests may prompt models and obtaining
    // the result can take a long time.
    if (IsGlobalCacheListFeatureEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{safe_browsing::kShowWarningsForSuspiciousNotifications,
            {{"ShowWarningsForSuspiciousNotificationsScoreThreshold", "0"}}},
           {safe_browsing::kReportNotificationContentDetectionData, {}},
           {kGlobalCacheListForGatingNotificationProtections, {}}},
          /*disabled_features=*/{optimization_guide::features::
                                     kPreventLongRunningPredictionModels});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{safe_browsing::kShowWarningsForSuspiciousNotifications,
            {{"ShowWarningsForSuspiciousNotificationsScoreThreshold", "0"}}},
           {safe_browsing::kReportNotificationContentDetectionData, {}}},
          /*disabled_features=*/{
              optimization_guide::features::kPreventLongRunningPredictionModels,
              kGlobalCacheListForGatingNotificationProtections});
    }

    NotificationContentDetectionBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    // Mock the `ModelQualityLogsUploaderService`.
    auto logs_uploader = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(
        g_browser_process->local_state());
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::move(logs_uploader));

    NotificationContentDetectionBrowserTest::SetUpOnMainThread();
  }

  bool IsGlobalCacheListFeatureEnabled() { return GetParam(); }

  void DisplayPersistentNotification(bool is_allowlisted) {
    blink::PlatformNotificationData data =
        CreateNotificationData(base::UTF8ToUTF16(notification_title),
                               base::UTF8ToUTF16(notification_message), {});
    GURL origin =
        (is_allowlisted ? GURL(kAllowlistedUrl) : GURL(kNonAllowlistedUrl));

    // Store notification data in `NotificationDatabase`.
    content::NotificationDatabaseData notification_database_data;
    notification_database_data.notification_data.title =
        base::UTF8ToUTF16(notification_title);
    notification_database_data.notification_data.body =
        base::UTF8ToUTF16(notification_message);
    notification_database_data.origin = origin;

    base::RunLoop run_loop;
    browser()
        ->profile()
        ->GetStoragePartitionForUrl(origin)
        ->GetPlatformNotificationContext()
        ->WriteNotificationData(
            notification_id, kFakeServiceWorkerRegistrationId, origin,
            notification_database_data,
            base::IgnoreArgs<bool, const std::string&>(run_loop.QuitClosure()));

    run_loop.Run();

    service()->DisplayPersistentNotification(
        GetNotificationId(/*is_allowlisted=*/true),
        origin /* service_worker_scope */, origin, data,
        blink::NotificationResources());
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester(),
        "OptimizationGuide.ModelExecutor.ExecutionStatus."
        "NotificationContentDetection",
        1);
  }

  optimization_guide::TestModelQualityLogsUploaderService* logs_uploader() {
    return static_cast<
        optimization_guide::TestModelQualityLogsUploaderService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile())
            ->GetModelQualityLogsUploaderService());
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  uploaded_logs() {
    return logs_uploader()->uploaded_logs();
  }

  void VerifyUniqueQualityLog(
      bool did_user_always_allow_url,
      bool is_url_on_allowlist,
      bool was_user_shown_warning,
      bool did_user_unsubscribe,
      optimization_guide::proto::SiteEngagementScore site_engagement_score) {
    const auto& logs = uploaded_logs();
    ASSERT_EQ(1u, logs.size());
    auto* const notification_content_detection =
        logs[0]->mutable_notification_content_detection();
    ASSERT_TRUE(notification_content_detection->has_request());
    ASSERT_TRUE(notification_content_detection->has_response());
    ASSERT_TRUE(notification_content_detection->has_quality());
    // Verify notification contents get logged.
    EXPECT_EQ(notification_title, notification_content_detection->request()
                                      .notification_contents()
                                      .notification_title());
    EXPECT_EQ(notification_message, notification_content_detection->request()
                                        .notification_contents()
                                        .notification_message());
    EXPECT_EQ(((did_user_always_allow_url || is_url_on_allowlist)
                   ? kAllowlistedUrl
                   : kNonAllowlistedUrl),
              notification_content_detection->request()
                  .notification_contents()
                  .url());
    // Verify suspicious score between 0 and 100 gets logged.
    EXPECT_GE(notification_content_detection->response().suspicious_score(), 0);
    EXPECT_LE(notification_content_detection->response().suspicious_score(),
              100);
    // Verify metadata gets logged.
    EXPECT_EQ(
        did_user_always_allow_url,
        notification_content_detection->quality().did_user_always_allow_url());
    EXPECT_EQ(is_url_on_allowlist,
              notification_content_detection->quality().is_url_on_allowlist());
    EXPECT_EQ(
        was_user_shown_warning,
        notification_content_detection->quality().was_user_shown_warning());
    EXPECT_EQ(did_user_unsubscribe,
              notification_content_detection->quality().did_user_unsubscribe());
    EXPECT_EQ(
        site_engagement_score,
        notification_content_detection->quality().site_engagement_score());
  }

  std::string GetNotificationId(bool is_allowlisted) {
    return "p#" +
           std::string(is_allowlisted ? kAllowlistedUrl : kNonAllowlistedUrl) +
           "#0" + base::NumberToString(notification_id);
  }

 private:
  std::string notification_title = "Title";
  std::string notification_message = "Message";
  const int64_t kFakeServiceWorkerRegistrationId = 42;
  int notification_id = 1;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    NotificationContentDetectionLoggingEnabledBrowserTest,
    testing::Bool());

IN_PROC_BROWSER_TEST_P(NotificationContentDetectionLoggingEnabledBrowserTest,
                       ReportUnwarnedNotificationAsSpam) {
  // Display notification with no warning.
  UpdateNotificationContentDetectionModel();
  DisplayPersistentNotification(/*is_allowlisted=*/true);
  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  bool did_show_warning =
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]);
  EXPECT_FALSE(did_show_warning);

  // Report notification as spam and verify MQLS log.
  bool did_user_unsubscribe = true;
  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->ReportUnwarnedNotificationAsSpam(
      GetNotificationId(/*is_allowlisted=*/true), GURL(kAllowlistedUrl),
      browser()->profile());
  ASSERT_TRUE(log_uploaded_signal.Wait());
  VerifyUniqueQualityLog(
      /*did_user_always_allow_url=*/false,
      /*is_url_on_allowlist=*/true, did_show_warning, did_user_unsubscribe,
      /*site_engagement_score=*/
      optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_NONE);
}

IN_PROC_BROWSER_TEST_P(NotificationContentDetectionLoggingEnabledBrowserTest,
                       ReportWarnedNotificationAsSpam) {
  // Display notification with warning.
  UpdateNotificationContentDetectionModel();
  DisplayPersistentNotification(/*is_allowlisted=*/false);
  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  bool did_show_warning =
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]);
  EXPECT_TRUE(did_show_warning);

  // Report notification as spam and verify MQLS log.
  bool did_user_unsubscribe = true;
  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->ReportWarnedNotificationAsSpam(
      GetNotificationId(/*is_allowlisted=*/false), GURL(kNonAllowlistedUrl),
      browser()->profile());
  ASSERT_TRUE(log_uploaded_signal.Wait());
  VerifyUniqueQualityLog(
      /*did_user_always_allow_url=*/false,
      /*is_url_on_allowlist=*/false, did_show_warning, did_user_unsubscribe,
      /*site_engagement_score=*/
      optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_NONE);
}

IN_PROC_BROWSER_TEST_P(NotificationContentDetectionLoggingEnabledBrowserTest,
                       ReportNotificationAsSafe) {
  // Display notification with warning.
  UpdateNotificationContentDetectionModel();
  DisplayPersistentNotification(/*is_allowlisted=*/false);
  EXPECT_EQ(GetDisplayedPersistentNotifications().size(), 1U);
  bool did_show_warning =
      IsNotificationSuspicious(GetDisplayedPersistentNotifications()[0]);
  EXPECT_TRUE(did_show_warning);

  // Report notification as safe and verify MQLS log.
  bool did_user_unsubscribe = false;
  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader()->WaitForLogUpload(log_uploaded_signal.GetCallback());
  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->ReportNotificationAsSafe(GetNotificationId(/*is_allowlisted=*/false),
                                    GURL(kNonAllowlistedUrl),
                                    browser()->profile());
  ASSERT_TRUE(log_uploaded_signal.Wait());
  VerifyUniqueQualityLog(
      /*did_user_always_allow_url=*/false,
      /*is_url_on_allowlist=*/false, did_show_warning, did_user_unsubscribe,
      /*site_engagement_score=*/
      optimization_guide::proto::SiteEngagementScore::
          SITE_ENGAGEMENT_SCORE_NONE);
}

}  // namespace safe_browsing
