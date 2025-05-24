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
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/mock_safe_browsing_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

const char kSbIncidentReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/incident";

class NotificationTelemetryServiceTest : public ::testing::Test {
 public:
  NotificationTelemetryServiceTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kNotificationTelemetry);
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
    notification_telemetry_service_ =
        std::make_unique<NotificationTelemetryService>(
            &profile_, test_url_loader_factory_.GetSafeWeakWrapper(),
            database_manager_);
  }

  void TearDown() override { notification_telemetry_service_.reset(); }

  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager() {
    return database_manager_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  std::unique_ptr<NotificationTelemetryService> notification_telemetry_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NotificationTelemetryServiceTest, Initializes) {
  ASSERT_NE(notification_telemetry_service_.get(), nullptr);
}

TEST_F(NotificationTelemetryServiceTest, SendsTelemetryReport) {
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

  database_manager()->SetAllowlistLookupDetailsForUrl(scope2, /*match=*/true);
  notification_telemetry_service_->OnRegistrationStored(
      registration_id_2, scope2, service_worker_info_2);
  database_manager()->SetAllowlistLookupDetailsForUrl(scope, /*match=*/false);
  notification_telemetry_service_->OnRegistrationStored(
      registration_id_1, scope, service_worker_info_1);
  database_manager()->SetAllowlistLookupDetailsForUrl(scope3, /*match=*/false);
  notification_telemetry_service_->OnRegistrationStored(
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

  test_url_loader_factory_.AddResponse(kSbIncidentReportUrl, "", net::HTTP_OK);
  notification_telemetry_service_->OnNewNotificationServiceWorkerSubscription(
      registration_id_1);
  notification_telemetry_service_->OnNewNotificationServiceWorkerSubscription(
      registration_id_2);
  notification_telemetry_service_->OnNewNotificationServiceWorkerSubscription(
      registration_id_3);

  // One pending as scope1 and scope3 SWs did not generate report data.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kSbIncidentReportUrl, "", net::HTTP_OK,
      network::TestURLLoaderFactory::kUrlMatchPrefix);
  // One report generated.
  histogram_tester().ExpectUniqueSample(
      /*name=*/"SafeBrowsing.NotificationTelemetry.NetworkResult",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
}

TEST_F(NotificationTelemetryServiceTest, EnforcesServiceWorkerInfoCacheSize) {
  const GURL scope("https://nonallowlisted_url.com/");
  const GURL scope2("https://nonallowlisted_url_2.com/");
  const GURL import_URL("https://import_script.com/script.js");

  int64_t registration_id_1 = 0451;
  int64_t registration_id_2 = 0452;

  // This service worker will be fully reported.
  content::ServiceWorkerRegistrationInformation service_worker_info_1;
  service_worker_info_1.resources.push_back(import_URL);

  database_manager()->SetAllowlistLookupDetailsForUrl(scope, /*match=*/false);
  notification_telemetry_service_->OnRegistrationStored(
      registration_id_1, scope, service_worker_info_1);

  // Second valid service worker info
  database_manager()->SetAllowlistLookupDetailsForUrl(scope2, /*match=*/false);
  content::ServiceWorkerRegistrationInformation service_worker_info_2;
  service_worker_info_2.resources.push_back(import_URL);

  // Fill the Notification Telemetry Service service worker to the size of
  // the cache. This will cause the first SW registration to be evicted.
  int cache_size =
      NotificationTelemetryService::ServiceWorkerInfoCacheSizeForTest();
  for (int counter = 0; counter < cache_size; counter++) {
    notification_telemetry_service_->OnRegistrationStored(
        (registration_id_2 + counter), scope2, service_worker_info_2);
  }

  // Try to trigger an upload of the first SW worker. Since the service worker
  // info cache no longer contains a corresponding entry, there should be no
  // match and hence no upload of a report will be attempted.
  notification_telemetry_service_->OnNewNotificationServiceWorkerSubscription(
      registration_id_1);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

}  // namespace safe_browsing
