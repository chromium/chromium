// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service.h"

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

}  // namespace safe_browsing
