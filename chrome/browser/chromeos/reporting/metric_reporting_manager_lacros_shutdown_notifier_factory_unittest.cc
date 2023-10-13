// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_shutdown_notifier_factory.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting::metrics {
namespace {

constexpr std::string_view kTestUserId = "123";

// Fake delegate implementation for the `MetricReportingManagerLacros`
// component. Used with the `MetricReportingManagerLacrosFactory` to block
// initialization of downstream components and simplify testing.
class FakeDelegate : public MetricReportingManagerLacros::Delegate {
 public:
  FakeDelegate() = default;
  FakeDelegate(const FakeDelegate& other) = delete;
  FakeDelegate& operator=(const FakeDelegate& other) = delete;
  ~FakeDelegate() override = default;

  bool IsUserAffiliated(Profile& profile) const override { return false; }

  std::unique_ptr<DeviceReportingSettingsLacros> CreateDeviceReportingSettings()
      override {
    return std::unique_ptr<DeviceReportingSettingsLacros>(nullptr);
  }
};

class MetricReportingManagerLacrosShutdownNotifierFactoryTest
    : public ::testing::Test {
 protected:
  MetricReportingManagerLacrosShutdownNotifierFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &MetricReportingManagerLacrosShutdownNotifierFactoryTest::
                    SetTestingFactory,
                base::Unretained(this)));

    // Set up main user profile. Used to monitor `MetricReportingManagerLacros`
    // component shutdown from the notifier.
    profile_ = profile_manager_.CreateTestingProfile(std::string{kTestUserId});
    profile_->SetIsMainProfile(true);
  }

  void SetTestingFactory(::content::BrowserContext* context) {
    MetricReportingManagerLacrosFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &MetricReportingManagerLacrosShutdownNotifierFactoryTest::
                         CreateMetricReportingManager,
                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMetricReportingManager(
      ::content::BrowserContext* context) {
    auto fake_delegate = std::make_unique<FakeDelegate>();
    return std::make_unique<MetricReportingManagerLacros>(
        static_cast<Profile*>(context), std::move(fake_delegate));
  }

  ::content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_;
  base::CallbackListSubscription dependency_manager_subscription_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(MetricReportingManagerLacrosShutdownNotifierFactoryTest,
       NotifyOnShutdown) {
  int callback_triggered_count = 0;
  const base::CallbackListSubscription shutdown_subscription =
      MetricReportingManagerLacrosShutdownNotifierFactory::GetInstance()
          ->Get(profile_.get())
          ->Subscribe(base::BindLambdaForTesting(
              [&callback_triggered_count]() { ++callback_triggered_count; }));
  ASSERT_THAT(callback_triggered_count, Eq(0));

  // Delete profile to trigger shutdown of the metric reporting manager and
  // verify callback was triggered.
  profile_ = nullptr;
  profile_manager_.DeleteAllTestingProfiles();
  EXPECT_THAT(callback_triggered_count, Eq(1));
}

}  // namespace
}  // namespace reporting::metrics
