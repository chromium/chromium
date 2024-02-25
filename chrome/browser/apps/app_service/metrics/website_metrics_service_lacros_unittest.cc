// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {
namespace {

// Test user id.
constexpr char kTestUserId[] = "123";

// Mock observer used to observe `WebsiteMetricsServiceLacros` for testing
// purposes.
class MockObserver : public WebsiteMetricsServiceLacros::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnWebsiteMetricsInit,
              (WebsiteMetrics * website_metrics),
              (override));

  MOCK_METHOD(void,
              OnWebsiteMetricsServiceLacrosWillBeDestroyed,
              (),
              (override));
};

class WebsiteMetricsServiceLacrosTest : public ::testing::Test {
 protected:
  WebsiteMetricsServiceLacrosTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kTestUserId);
    metrics_service_ = std::make_unique<WebsiteMetricsServiceLacros>(profile_);
    metrics_service_->AddObserver(&observer_);
  }

  void TearDown() override {
    if (metrics_service_) {
      // Unregister observer to eliminate noise during teardown.
      metrics_service_->RemoveObserver(&observer_);
    }
  }

  ::content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<WebsiteMetricsServiceLacros> metrics_service_;
  MockObserver observer_;
};

TEST_F(WebsiteMetricsServiceLacrosTest, NotifyObserverOnWebsiteMetricsInit) {
  auto website_metrics = std::make_unique<WebsiteMetrics>(
      profile_, /*user_type_by_device_type=*/0);
  auto* const website_metrics_ptr = website_metrics.get();
  metrics_service_->SetWebsiteMetricsForTesting(std::move(website_metrics));

  EXPECT_CALL(observer_, OnWebsiteMetricsInit(website_metrics_ptr)).Times(1);
  metrics_service_->Start();
}

TEST_F(WebsiteMetricsServiceLacrosTest,
       DoNotNotifyObserversOnWebsiteMetricsInitIfUnregistered) {
  // Unregister observer.
  metrics_service_->RemoveObserver(&observer_);

  auto website_metrics = std::make_unique<WebsiteMetrics>(
      profile_, /*user_type_by_device_type=*/0);
  metrics_service_->SetWebsiteMetricsForTesting(std::move(website_metrics));

  EXPECT_CALL(observer_, OnWebsiteMetricsInit).Times(0);
  metrics_service_->Start();
}

TEST_F(WebsiteMetricsServiceLacrosTest, NotifyObserverOnDestruction) {
  EXPECT_CALL(observer_, OnWebsiteMetricsServiceLacrosWillBeDestroyed).Times(1);
  metrics_service_.reset();
}

}  // namespace
}  // namespace apps
