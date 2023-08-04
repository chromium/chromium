// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
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

void LockAndUnlockSession() {
  const AccountId account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  ash::ScreenLockerTester tester;
  tester.Lock();
  EXPECT_TRUE(tester.IsLocked());
  tester.SetUnlockPassword(account_id, "pass");
  tester.UnlockWithPassword(account_id, "pass");
  tester.WaitForUnlock();
  EXPECT_FALSE(tester.IsLocked());
}

void SendSuspendDone() {
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
}

class ScalableIphBrowserTestVersionNumberNoValue
    : public ScalableIphBrowserTest {
 protected:
  void AppendVersionNumber(base::FieldTrialParams& params) override {}
};

class ScalableIphBrowserTestVersionNumberIncorrect
    : public ScalableIphBrowserTest {
 protected:
  void AppendVersionNumber(base::FieldTrialParams& params) override {
    params[FullyQualified(TestIphFeature(),
                          scalable_iph::kCustomParamsVersionNumberParamName)] =
        base::NumberToString(scalable_iph::kCurrentVersionNumber - 1);
  }
};

class ScalableIphBrowserTestVersionNumberInvalid
    : public ScalableIphBrowserTest {
 protected:
  void AppendVersionNumber(base::FieldTrialParams& params) override {
    params[FullyQualified(TestIphFeature(),
                          scalable_iph::kCustomParamsVersionNumberParamName)] =
        "Invalid";
  }
};

class ScalableIphBrowserTestNetworkConnection : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendVersionNumber(params);
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
    AppendVersionNumber(params);
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
    AppendVersionNumber(params);
    AppendFakeUiParamsBubble(params);
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, test_config}, {});
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestFlagOff, HasServiceWhenFeatureEnabled) {
  if (ash::features::IsScalableIphEnabled()) {
    EXPECT_TRUE(ScalableIphFactory::GetForBrowserContext(browser()->profile()));
  } else {
    EXPECT_FALSE(
        ScalableIphFactory::GetForBrowserContext(browser()->profile()));
  }
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, RecordEvent_FiveMinTick) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, RecordEvent_Unlocked) {
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked));

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kUnlocked);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, InvokeIphByTimer_Notification) {
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
  expected_params.button.action.action_type =
      scalable_iph::ActionType::kOpenChrome;
  expected_params.button.action.iph_event_name =
      ScalableIphBrowserTestBase::kTestButtonActionEvent;

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

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, InvokeIphByUnlock_Notification) {
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
  expected_params.button.action.action_type =
      scalable_iph::ActionType::kOpenChrome;
  expected_params.button.action.iph_event_name =
      ScalableIphBrowserTestBase::kTestButtonActionEvent;

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
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kUnlocked);
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

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, NoTimeTickEventWithLockScreen) {
  // We test unlocked event inside ScalableIph service. Make sure that
  // ScalableIph service is running.
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
  // But a time tick event will not be observed because device is locked.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameFiveMinTick))
      .Times(0);
  ash::ScreenLockerTester tester;
  tester.Lock();
  task_runner()->FastForwardBy(base::Minutes(3));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

// TODO(crbug.com/1468580): Flaky test.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, DISABLED_UnlockedEvent) {
  // We test unlocked event inside ScalableIph service. Make sure that
  // ScalableIph service is running.
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(scalable_iph);

  // No Unlocked event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(0);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Lock and unlock screen. An Unlocked event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(1);
  LockAndUnlockSession();
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Shutdown should stop the observations and no Unlocked event should be
  // observed.
  ShutdownScalableIph();
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(0);
  LockAndUnlockSession();
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, OnSuspendDone) {
  // We test unlocked event inside ScalableIph service. Make sure that
  // ScalableIph service is running.
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(scalable_iph);

  // No Unlocked event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(0);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Simulate SuspendDone. An Unlocked event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(1);
  SendSuspendDone();
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Shutdown should stop the observations and no Unlocked event should be
  // observed.
  ShutdownScalableIph();
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(0);
  SendSuspendDone();
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, OnSuspendDoneWithLockScreen) {
  // We test unlocked event inside ScalableIph service. Make sure that
  // ScalableIph service is running.
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(scalable_iph);

  // No Unlocked event should be observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(0);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Simulate SuspendDone with lock screen. No Unlocked event should be
  // observed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(scalable_iph::kEventNameUnlocked))
      .Times(0);
  ash::ScreenLockerTester tester;
  tester.Lock();
  SendSuspendDone();
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, AppListShown) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameAppListShown));

  ash::AppListController* app_list_controller = ash::AppListController::Get();
  CHECK(app_list_controller);
  app_list_controller->ShowAppList(ash::AppListShowSource::kSearchKey);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestVersionNumberNoValue, NoValue) {
  EnableTestIphFeature();

  // No trigger condition check should happen if it fails to validate a version
  // number as the config gets skipped.
  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(0);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestVersionNumberIncorrect,
                       Incorrect) {
  EnableTestIphFeature();

  // No trigger condition check should happen if it fails to validate a version
  // number as the config gets skipped.
  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(0);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestVersionNumberInvalid, Invalid) {
  EnableTestIphFeature();

  // No trigger condition check should happen if it fails to validate a version
  // number as the config gets skipped.
  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(0);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
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
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeZero, Satisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeZero,
                       NotSatisfiedAboveThreshold) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(25));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeZero,
                       NotSatisfiedFutureCreationDate) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() +
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeNonZero, Satisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(47));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeNonZero, NotSatisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(49));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeInvalidString,
                       NotSatisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestClientAgeInvalidNumber,
                       NotSatisfied) {
  EnableTestIphFeature();
  browser()->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                  base::Hours(1));
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNotification, ShowNotification) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is not performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent)).Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  auto* message_center = message_center::MessageCenter::Get();
  auto* notification =
      message_center->FindVisibleNotificationById(kTestNotificationId);
  EXPECT_TRUE(notification);
  message_center->RemoveNotification(kTestNotificationId,
                                     /*by_user=*/false);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNotification,
                       ClickNotificationButton) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent));

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  auto* message_center = message_center::MessageCenter::Get();
  auto* notification =
      message_center->FindVisibleNotificationById(kTestNotificationId);
  EXPECT_TRUE(notification);
  EXPECT_TRUE(notification->delegate());
  notification->delegate()->Click(/*button_index=*/0, /*reply=*/absl::nullopt);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, InvokeIphByTimer_Bubble) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  scalable_iph::ScalableIphDelegate::BubbleParams expected_params;
  expected_params.bubble_id = ScalableIphBrowserTestBase::kTestBubbleId;
  expected_params.text = ScalableIphBrowserTestBase::kTestBubbleText;
  expected_params.button.text =
      ScalableIphBrowserTestBase::kTestBubbleButtonText;
  expected_params.button.action.action_type =
      scalable_iph::ActionType::kOpenGoogleDocs;
  expected_params.button.action.iph_event_name =
      ScalableIphBrowserTestBase::kTestButtonActionEvent;
  expected_params.icon =
      scalable_iph::ScalableIphDelegate::BubbleIcon::kGoogleDocsIcon;

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
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, InvokeIphByUnlock_Bubble) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  scalable_iph::ScalableIphDelegate::BubbleParams expected_params;
  expected_params.bubble_id = ScalableIphBrowserTestBase::kTestBubbleId;
  expected_params.text = ScalableIphBrowserTestBase::kTestBubbleText;
  expected_params.button.text =
      ScalableIphBrowserTestBase::kTestBubbleButtonText;
  expected_params.button.action.action_type =
      scalable_iph::ActionType::kOpenGoogleDocs;
  expected_params.button.action.iph_event_name =
      ScalableIphBrowserTestBase::kTestButtonActionEvent;
  expected_params.icon =
      scalable_iph::ScalableIphDelegate::BubbleIcon::kGoogleDocsIcon;

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
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kUnlocked);
}

// TODO(b/290307529): Fix the test.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, DISABLED_ShowBubble) {
  EnableTestIphFeature();
  mock_delegate()->FakeShowBubble();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is not performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent)).Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  // Default nudge duration is 6 seconds.
  task_runner()->FastForwardBy(base::Seconds(7));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
  // TODO(b/290066999): Verify the nudge is shown.
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, RemoveBubble) {
  EnableTestIphFeature();
  mock_delegate()->FakeShowBubble();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is not performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent)).Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  ash::AnchoredNudgeManager::Get()->Cancel(kTestBubbleId);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
  // TODO(b/290066999): Verify the nudge is not shown.
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
