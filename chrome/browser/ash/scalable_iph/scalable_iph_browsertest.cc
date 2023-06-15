// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ScalableIphBrowserTestFlagOff = ::ash::CustomizableTestEnvBrowserTestBase;
using ScalableIphBrowserTest = ::ash::ScalableIphBrowserTestBase;
using TestEnvironment =
    ::ash::CustomizableTestEnvBrowserTestBase::TestEnvironment;
using UserSessionType =
    ::ash::CustomizableTestEnvBrowserTestBase::UserSessionType;

class ScalableIphBrowserTestParameterized
    : public ash::CustomizableTestEnvBrowserTestBase,
      public testing::WithParamInterface<TestEnvironment> {
 public:
  void SetUp() override {
    SetTestEnvironment(GetParam());

    ash::CustomizableTestEnvBrowserTestBase::SetUp();
  }
};

BASE_FEATURE(kScalableIphTest,
             "ScalableIphTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kFiveMinTickEventName[] = "ScalableIphFiveMinTick";

}  // namespace

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestFlagOff, NoService) {
  EXPECT_FALSE(ash::features::IsScalableIphEnabled());
  EXPECT_FALSE(ScalableIphFactory::GetForBrowserContext(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, RecordEvent) {
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kFiveMinTickEventName));

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, InvokeIph) {
  ON_CALL(*mock_tracker(), ShouldTriggerHelpUI)
      .WillByDefault([](const base::Feature& feature) {
        return &feature == &kScalableIphTest;
      });

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(kScalableIphTest)));

  scalable_iph::ScalableIphDelegate::BubbleParams expected_params;
  EXPECT_CALL(*mock_delegate(),
              ShowBubble(::testing::Eq(expected_params), ::testing::NotNull()))
      .WillOnce(
          [](const scalable_iph::ScalableIphDelegate::BubbleParams& params,
             std::unique_ptr<scalable_iph::IphSession> session) {
            // Simulate that an IPH gets dismissed.
            session.reset();
          });

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  std::vector<const base::Feature*> features = {&kScalableIphTest};
  scalable_iph->OverrideFeatureListForTesting(features);

  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, TimeTickEvent) {
  // We test a timer inside ScalableIph service. Make sure that ScalableIph
  // service is running.
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(scalable_iph);

  base::TestMockTimeTaskRunner::ScopedContext context(task_runner());

  // Fast forward by 3 mins. The interval of time tick event is 5 mins. No time
  // tick event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kFiveMinTickEventName)).Times(0);
  task_runner()->FastForwardBy(base::Minutes(3));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Fast forward by another 3 mins. The total of fast forwarded time is 6 mins.
  // A time tick event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kFiveMinTickEventName)).Times(1);
  task_runner()->FastForwardBy(base::Minutes(3));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  ShutdownScalableIph();

  // Fast forward by another 6 mins after the shutdown. Shutdown should stop the
  // timer and no time tick event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kFiveMinTickEventName)).Times(0);
  task_runner()->FastForwardBy(base::Minutes(6));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

INSTANTIATE_TEST_SUITE_P(
    NoScalableIph,
    ScalableIphBrowserTestParameterized,
    testing::Values(
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED,
            UserSessionType::kManaged),
        // A test case where a regular profile on a managed device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED,
            UserSessionType::kRegular),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kGuest),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kChild),
        // A test case where a child profile is an owner of a device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kChildOwner),
        // A Test case where a managed account is an owner of an un-enrolled
        // device.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kManaged),
        // A test case where a regular profile is not an owner profile.
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kRegularNonOwner)),
    &TestEnvironment::GenerateTestName);

IN_PROC_BROWSER_TEST_P(ScalableIphBrowserTestParameterized,
                       ScalableIphNotAvailable) {
  EXPECT_EQ(nullptr,
            ScalableIphFactory::GetForBrowserContext(browser()->profile()));
}

// TODO(b/284053005): Add a test case for invalid event name.
