// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_factory.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting::metrics {
namespace {

constexpr char kTestUserId[] = "123";

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

class MetricReportingManagerLacrosFactoryTest : public ::testing::Test {
 protected:
  MetricReportingManagerLacrosFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &MetricReportingManagerLacrosFactoryTest::SetTestingFactory,
                base::Unretained(this)));
    profile_ = profile_manager_.CreateTestingProfile(kTestUserId);
  }

  void SetTestingFactory(::content::BrowserContext* context) {
    MetricReportingManagerLacrosFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&MetricReportingManagerLacrosFactoryTest::
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

TEST_F(MetricReportingManagerLacrosFactoryTest, WithMainProfile) {
  profile_->SetIsMainProfile(true);
  EXPECT_NE(MetricReportingManagerLacrosFactory::GetForProfile(profile_.get()),
            nullptr);
}

TEST_F(MetricReportingManagerLacrosFactoryTest, WithSecondaryProfile) {
  profile_->SetIsMainProfile(false);
  EXPECT_EQ(MetricReportingManagerLacrosFactory::GetForProfile(profile_.get()),
            nullptr);
}

}  // namespace
}  // namespace reporting::metrics
