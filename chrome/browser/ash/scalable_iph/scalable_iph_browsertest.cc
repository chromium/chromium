// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

using ScalableIphBrowserTestFlagOff = ::ash::CustomizableTestEnvBrowserTestBase;
using ScalableIphBrowserTest = ::ash::ScalableIphBrowserTestBase;
using TestEnvironment =
    ::ash::CustomizableTestEnvBrowserTestBase::TestEnvironment;
using UserSessionType =
    ::ash::CustomizableTestEnvBrowserTestBase::UserSessionType;

class ScalableIphBrowserTestNetworkConnection : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendFakeUiParamsNotification(params);
    params[FullyQualified(
        TestIphFeature(),
        scalable_iph::kCustomConditionNetworkConnectionParamName)] =
        scalable_iph::kCustomConditionNetworkConnectionOnline;
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, test_config}, {});
  }
};

class ScalableIphBrowserTestNetworkConnectionOnline
    : public ScalableIphBrowserTestNetworkConnection {
 protected:
  void SetUpOnMainThread() override {
    AddOnlineNetwork();

    ScalableIphBrowserTestNetworkConnection::SetUpOnMainThread();
  }
};

class ScalableIphBrowserTestClientAgeBase : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendFakeUiParamsNotification(params);
    params[FullyQualified(
        TestIphFeature(),
        scalable_iph::kCustomConditionClientAgeInDaysParamName)] =
        GetClientAgeTestValue();
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, test_config}, {});
  }

  void SetUpOnMainThread() override {
    ScalableIphBrowserTest::SetUpOnMainThread();

    mock_delegate()->FakeClientAgeInDays();
  }

  virtual std::string GetClientAgeTestValue() = 0;
};

class ScalableIphBrowserTestClientAgeZero
    : public ScalableIphBrowserTestClientAgeBase {
 protected:
  // Day 0 is from 0 hours to 24 hours.
  std::string GetClientAgeTestValue() override { return "0"; }
};

class ScalableIphBrowserTestClientAgeNonZero
    : public ScalableIphBrowserTestClientAgeBase {
 protected:
  // Day 1 is from 24 hours to 48 hours.
  std::string GetClientAgeTestValue() override { return "1"; }
};

class ScalableIphBrowserTestClientAgeInvalidString
    : public ScalableIphBrowserTestClientAgeBase {
 protected:
  std::string GetClientAgeTestValue() override { return "abc"; }
};

class ScalableIphBrowserTestClientAgeInvalidNumber
    : public ScalableIphBrowserTestClientAgeBase {
 protected:
  std::string GetClientAgeTestValue() override { return "-1"; }
};

class ScalableIphBrowserTestParameterized
    : public ash::CustomizableTestEnvBrowserTestBase,
      public testing::WithParamInterface<TestEnvironment> {
 public:
  void SetUp() override {
    SetTestEnvironment(GetParam());

    ash::CustomizableTestEnvBrowserTestBase::SetUp();
  }
};

class MockMessageCenterObserver
    : public testing::NiceMock<message_center::MessageCenterObserver> {
 public:
  // MessageCenterObserver:
  MOCK_METHOD(void,
              OnNotificationAdded,
              (const std::string& notification_id),
              (override));

  MOCK_METHOD(void,
              OnNotificationUpdated,
              (const std::string& notification_id),
              (override));
};

class ScalableIphBrowserTestNotification : public ScalableIphBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ScalableIphBrowserTest::SetUpOnMainThread();

    auto* message_center = message_center::MessageCenter::Get();
    scoped_observation_.Observe(message_center);
    EXPECT_CALL(mock_, OnNotificationAdded(kTestNotificationId));

    mock_delegate()->FakeShowNotification();
  }

  void TearDownOnMainThread() override {
    scoped_observation_.Reset();

    ScalableIphBrowserTest::TearDownOnMainThread();
  }

 private:
  // Observe notifications.
  MockMessageCenterObserver mock_;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      scoped_observation_{&mock_};
};

class ScalableIphBrowserTestBubble : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendFakeUiParamsBubble(params);
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, test_config}, {});
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestFlagOff, NoService) {
  EXPECT_FALSE(ash::features::IsScalableIphEnabled());
  EXPECT_FALSE(ScalableIphFactory::GetForBrowserContext(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, RecordEvent) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, InvokeIph) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  scalable_iph::ScalableIphDelegate::NotificationParams expected_params;
  expected_params.notification_id =
      ScalableIphBrowserTestBase::kTestNotificationId;
  expected_params.title = ScalableIphBrowserTestBase::kTestNotificationTitle;
  expected_params.text = ScalableIphBrowserTestBase::kTestNotificationBodyText;
  expected_params.button.text =
      ScalableIphBrowserTestBase::kTestNotificationButtonText;

  EXPECT_CALL(*mock_delegate(), ShowNotification(::testing::Eq(expected_params),
                                                 ::testing::NotNull()))
      .WillOnce([](const scalable_iph::ScalableIphDelegate::NotificationParams&
                       params,
                   std::unique_ptr<scalable_iph::IphSession> session) {
        // Simulate that an IPH gets dismissed.
        session.reset();
      });
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, TimeTickEvent) {
  // We test a timer inside ScalableIph service. Make sure that ScalableIph
  // service is running.
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(scalable_iph);

  // Fast forward by 3 mins. The interval of time tick event is 5 mins. No time
  // tick event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameFiveMinTick))
      .Times(0);
  task_runner()->FastForwardBy(base::Minutes(3));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Fast forward by another 3 mins. The total of fast forwarded time is 6 mins.
  // A time tick event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameFiveMinTick))
      .Times(1);
  task_runner()->FastForwardBy(base::Minutes(3));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  ShutdownScalableIph();

  // Fast forward by another 6 mins after the shutdown. Shutdown should stop the
  // timer and no time tick event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameFiveMinTick))
      .Times(0);
  task_runner()->FastForwardBy(base::Minutes(6));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNetworkConnection, Online) {
  EnableTestIphFeature();

  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);

  AddOnlineNetwork();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNetworkConnectionOnline,
                       OnlineFromBeginning) {
  EnableTestIphFeature();

  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);

  // We have to trigger a conditions check manually. The trigger condition check
  // in `ScalableIph` constructor happens before we set the expectation to the
  // delegate mock. We need another event for the next check.
  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeZero, Satisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);

  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeZero,
                       NotSatisfiedAboveThreshold) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(25));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeZero,
                       NotSatisfiedFutureCreationDate) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() +
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeNonZero, Satisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(47));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);

  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeNonZero, NotSatisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(49));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeInvalidString,
                       NotSatisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeInvalidNumber,
                       NotSatisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNotification, ShowNotification) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);

  auto* message_center = message_center::MessageCenter::Get();
  auto* notification =
      message_center->FindVisibleNotificationById(kTestNotificationId);
  EXPECT_TRUE(notification);
  message_center->RemoveNotification(kTestNotificationId,
                                     /*by_user=*/false);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNotification,
                       ClickNotificationButton) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);

  auto* message_center = message_center::MessageCenter::Get();
  auto* notification =
      message_center->FindVisibleNotificationById(kTestNotificationId);
  EXPECT_TRUE(notification);
  EXPECT_TRUE(notification->delegate());
  notification->delegate()->Click(/*button_index=*/0, /*reply=*/absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, ShowBubble) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  TriggerConditionsCheckWithAFakeEvent();
  // TODO(b/290066999): Verify the nudge is shown.
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
