// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service.h"

#include <cstdint>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/mock_safe_browsing_database_manager.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/service_worker_registration_information.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::EqualsProto;
using ::testing::_;
using ::testing::Pointee;
namespace safe_browsing {

namespace {
CSBRR::ServiceWorkerBehavior MakeServiceWorkerRegistrationBehavior(
    const GURL& scope_url,
    const std::vector<GURL>& import_script_urls) {
  std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior =
      std::make_unique<CSBRR::ServiceWorkerBehavior>();

  service_worker_behavior->set_scope_url(scope_url.spec());

  for (const GURL& import_script_url : import_script_urls) {
    service_worker_behavior->add_import_script_urls(import_script_url.spec());
  }
  return *service_worker_behavior;
}
CSBRR::ServiceWorkerBehavior MakeServiceWorkerPushBehavior(
    const GURL& script_url,
    const std::vector<GURL>& requested_urls) {
  std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior =
      std::make_unique<CSBRR::ServiceWorkerBehavior>();

  service_worker_behavior->set_script_url(script_url.spec());

  for (const GURL& requested_url : requested_urls) {
    service_worker_behavior->add_requested_urls(requested_url.spec());
  }
  return *service_worker_behavior;
}

ClientSafeBrowsingReportRequest MakeCSBRR(
    const std::vector<CSBRR::ServiceWorkerBehavior>& service_worker_behaviors) {
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(CSBRR::SERVICE_WORKER_BEHAVIOR);
  for (const auto& behavior : service_worker_behaviors) {
    *report->add_service_worker_behaviors() = behavior;
    if (behavior.has_script_url()) {
      report->set_page_url(behavior.script_url());
    } else if (behavior.has_scope_url()) {
      report->set_page_url(behavior.scope_url());
    }
  }
  return *report;
}

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(
            std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
            chrome::ChromeUINewTabURLAsGURL()) {}

  MockSafeBrowsingUIManager(const MockSafeBrowsingUIManager&) = delete;
  MockSafeBrowsingUIManager& operator=(const MockSafeBrowsingUIManager&) =
      delete;

  MOCK_METHOD2(SendThreatDetails,
               void(content::BrowserContext* browser_context,
                    std::unique_ptr<ClientSafeBrowsingReportRequest> report));

 protected:
  ~MockSafeBrowsingUIManager() override = default;
};
}  // namespace

const char kSbIncidentReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/incident";

class NotificationTelemetryServiceTest : public ::testing::TestWithParam<bool> {
 public:
  NotificationTelemetryServiceTest() = default;

  bool IsGlobalCacheListFeatureEnabled() { return GetParam(); }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kNotificationTelemetry,
                              kNotificationTelemetrySwb},
        /*disabled_features=*/{});
    profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
    // TODO(crbug.com/433543634): Cleanup the use of `database_manager_` post
    // GlobalCacheListForGatingNotificationProtections launch.
    ui_manager_ = new MockSafeBrowsingUIManager();
    if (IsGlobalCacheListFeatureEnabled()) {
      notification_telemetry_service_ =
          std::make_unique<NotificationTelemetryService>(
              &profile_, test_url_loader_factory_.GetSafeWeakWrapper(), nullptr,
              ui_manager_);

    } else {
      database_manager_ = new MockSafeBrowsingDatabaseManager();
      // Create service.
      notification_telemetry_service_ =
          std::make_unique<NotificationTelemetryService>(
              &profile_, test_url_loader_factory_.GetSafeWeakWrapper(),
              database_manager_, ui_manager_);
    }
  }

  void TearDown() override { notification_telemetry_service_.reset(); }

  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager() {
    return database_manager_;
  }

  void SetAllowlistInfoForUrl(const GURL& scope, bool allowlisted) {
    if (IsGlobalCacheListFeatureEnabled()) {
      if (allowlisted) {
        SetNotificationsGlobalCacheListDomainsForTesting({scope.GetHost()});
      } else {
        // Populate with a dummy value because an empty list will cause scope
        // to be on the allowlist.
        SetNotificationsGlobalCacheListDomainsForTesting({"dummy.value.com"});
      }

    } else if (!IsGlobalCacheListFeatureEnabled()) {
      database_manager()->SetAllowlistLookupDetailsForUrl(scope, allowlisted);
    }
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  NotificationTelemetryService* notification_telemetry_service() {
    return notification_telemetry_service_.get();
  }
  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  scoped_refptr<MockSafeBrowsingUIManager> ui_manager() { return ui_manager_; }
  TestingProfile& profile() { return profile_; }

 private:
  std::unique_ptr<NotificationTelemetryService> notification_telemetry_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<MockSafeBrowsingUIManager> ui_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_P(NotificationTelemetryServiceTest, Initializes) {
  ASSERT_NE(notification_telemetry_service(), nullptr);
}

TEST_P(NotificationTelemetryServiceTest, SendsTelemetryReport) {
  const GURL scope("https://nonallowlisted_url.com/");
  const GURL scope2("https://allowlisted_url.com/");
  const GURL scope3("https://nonallowlisted_url_2.com/");
  const GURL import_URL("https://import_script.com/script.js");

  int64_t registration_id_1 = 0451;
  int64_t registration_id_2 = 0452;
  int64_t registration_id_3 = 0453;

  // This service worker will be fully reported.
  content::ServiceWorkerRegistrationInformation service_worker_info_1;
  service_worker_info_1.resources.push_back(import_URL);

  // This service worker will not be reported because it's scope is in
  // the allowlist.
  content::ServiceWorkerRegistrationInformation service_worker_info_2;
  service_worker_info_2.resources.push_back(import_URL);

  // This service worker will not be reported because it does not
  // import scripts from a different origin.
  content::ServiceWorkerRegistrationInformation service_worker_info_3;
  service_worker_info_3.resources.push_back(scope3);

  SetAllowlistInfoForUrl(scope2, /*allowlisted=*/true);
  notification_telemetry_service()->OnRegistrationStored(
      registration_id_2, scope2, service_worker_info_2);
  SetAllowlistInfoForUrl(scope, /*allowlisted=*/false);
  notification_telemetry_service()->OnRegistrationStored(
      registration_id_1, scope, service_worker_info_1);
  SetAllowlistInfoForUrl(scope3, /*allowlisted=*/false);
  notification_telemetry_service()->OnRegistrationStored(
      registration_id_3, scope3, service_worker_info_3);

  histogram_tester().ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.NotificationTelemetry.ServiceWorkerScopeURL.IsAllowlisted",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester().ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.NotificationTelemetry.ServiceWorkerScopeURL.IsAllowlisted",
      /*sample=*/false,
      /*expected_bucket_count=*/1);

  test_url_loader_factory().AddResponse(kSbIncidentReportUrl, "", net::HTTP_OK);
  notification_telemetry_service()->OnNewNotificationServiceWorkerSubscription(
      registration_id_1);
  notification_telemetry_service()->OnNewNotificationServiceWorkerSubscription(
      registration_id_2);
  notification_telemetry_service()->OnNewNotificationServiceWorkerSubscription(
      registration_id_3);

  // One pending as scope1 and scope3 SWs did not generate report data.
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  test_url_loader_factory().SimulateResponseForPendingRequest(
      kSbIncidentReportUrl, "", net::HTTP_OK,
      network::TestURLLoaderFactory::kUrlMatchPrefix);
  // One report generated.
  histogram_tester().ExpectUniqueSample(
      /*name=*/"SafeBrowsing.NotificationTelemetry.NetworkResult",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
}

TEST_P(NotificationTelemetryServiceTest, EnforcesServiceWorkerInfoCacheSize) {
  const GURL scope("https://nonallowlisted_url.com/");
  const GURL scope2("https://nonallowlisted_url_2.com/");
  const GURL import_URL("https://import_script.com/script.js");

  int64_t registration_id_1 = 0451;
  int64_t registration_id_2 = 0452;

  // This service worker will be fully reported.
  content::ServiceWorkerRegistrationInformation service_worker_info_1;
  service_worker_info_1.resources.push_back(import_URL);

  SetAllowlistInfoForUrl(scope, /*allowlisted=*/false);
  notification_telemetry_service()->OnRegistrationStored(
      registration_id_1, scope, service_worker_info_1);

  // Second valid service worker info
  SetAllowlistInfoForUrl(scope2, /*allowlisted=*/false);
  content::ServiceWorkerRegistrationInformation service_worker_info_2;
  service_worker_info_2.resources.push_back(import_URL);

  // Fill the Notification Telemetry Service service worker to the size of
  // the cache. This will cause the first SW registration to be evicted.
  int cache_size =
      NotificationTelemetryService::ServiceWorkerInfoCacheSizeForTest();
  for (int counter = 0; counter < cache_size; counter++) {
    notification_telemetry_service()->OnRegistrationStored(
        (registration_id_2 + counter), scope2, service_worker_info_2);
  }

  // Try to trigger an upload of the first SW worker. Since the service worker
  // info cache no longer contains a corresponding entry, there should be no
  // match and hence no upload of a report will be attempted.
  notification_telemetry_service()->OnNewNotificationServiceWorkerSubscription(
      registration_id_1);
  EXPECT_EQ(0, test_url_loader_factory().NumPending());
}

TEST_P(NotificationTelemetryServiceTest,
       ServiceWorkerSubscriptionSendsServiceWorkerBehavior) {
  const GURL scope("https://nonallowlisted_url.com/");
  const GURL import_URL("https://import_script.com/script.js");

  int64_t registration_id_1 = 0451;

  // This service worker will be fully reported.
  content::ServiceWorkerRegistrationInformation service_worker_info_1;
  service_worker_info_1.resources.push_back(import_URL);

  SetAllowlistInfoForUrl(scope, /*allowlisted=*/false);
  notification_telemetry_service()->OnRegistrationStored(
      registration_id_1, scope, service_worker_info_1);

  CSBRR::ServiceWorkerBehavior expected_behavior =
      MakeServiceWorkerRegistrationBehavior(scope,
                                            std::vector<GURL>{import_URL});
  auto expected_report = MakeCSBRR({expected_behavior});
  EXPECT_CALL(*ui_manager(),
              SendThreatDetails(_, Pointee(EqualsProto(expected_report))));

  notification_telemetry_service()->OnNewNotificationServiceWorkerSubscription(
      registration_id_1);
}

TEST_P(NotificationTelemetryServiceTest, OnPushEventFinished) {
  const GURL script_url("http://scope.com");
  std::vector<GURL> requested_urls = {
      GURL("http://dest.com?a=1&b=2"), GURL("http://dest.com?a=1&b=2"),
      GURL("http://dest.com"),         GURL("http://dest.com"),
      GURL("invalidurl.ini"),          GURL("")};

  // Duplicate requested URLs should be deduped and normalized.
  // Invalid requested URLs should be ignored.
  CSBRR::ServiceWorkerBehavior expected_behavior =
      MakeServiceWorkerPushBehavior(
          script_url, std::vector<GURL>{GURL("http://dest.com"),
                                        GURL("http://dest.com?a&b")});
  auto expected_report = MakeCSBRR({expected_behavior});
  EXPECT_CALL(*ui_manager(),
              SendThreatDetails(_, Pointee(EqualsProto(expected_report))));
  notification_telemetry_service()->SetShouldSendReportForTest(true);
  notification_telemetry_service()->OnPushEventFinished(script_url,
                                                        requested_urls);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         NotificationTelemetryServiceTest,
                         ::testing::Bool());

}  // namespace safe_browsing
