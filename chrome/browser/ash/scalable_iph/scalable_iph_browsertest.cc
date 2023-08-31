// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "base/strings/pattern.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/scalable_iph/scalable_iph_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/account_id/account_id.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
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

constexpr char kTestLogMessage[] = "test-log-message";
constexpr char kTestLogMessagePattern[] = "*test-log-message*";
constexpr char kScalableIphDebugLogTextUrl[] =
    "chrome-untrusted://scalable-iph-debug/log.txt";

BASE_FEATURE(kScalableIphTestTwo,
             "ScalableIphTestTwo",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGoogleChrome() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

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

class AppListItemWaiter : public AppListModelUpdaterObserver {
 public:
  AppListItemWaiter(std::string app_id,
                    AppListModelUpdater* app_list_model_updater)
      : app_id_(app_id), app_list_model_updater_(app_list_model_updater) {}

  void Wait() {
    if (app_list_model_updater_->FindItem(app_id_)) {
      return;
    }

    app_list_model_updater_observation_.Observe(app_list_model_updater_);
    run_loop_.Run();
  }

  void OnAppListItemAdded(ChromeAppListItem* item) override {
    if (item->id() == app_id_) {
      run_loop_.Quit();
    }
  }

 private:
  const std::string app_id_;
  raw_ptr<AppListModelUpdater> app_list_model_updater_;
  base::RunLoop run_loop_;
  base::ScopedObservation<AppListModelUpdater, AppListModelUpdaterObserver>
      app_list_model_updater_observation_{this};
};

class ScalableIphBrowserTestDebugOff : public ScalableIphBrowserTest {
 public:
  ScalableIphBrowserTestDebugOff() { enable_scalable_iph_debug_ = false; }
};

class ScalableIphBrowserTestPreinstallApps : public ScalableIphBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    ScalableIphBrowserTest::SetUpDefaultCommandLine(command_line);

    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }
};

class ScalableIphBrowserTestOobe : public ScalableIphBrowserTest {
 public:
  ScalableIphBrowserTestOobe() {
    SetTestEnvironment(TestEnvironment(
        ash::DeviceStateMixin::State::BEFORE_OOBE,
        CustomizableTestEnvBrowserTestBase::UserSessionType::kRegularWithOobe));
  }
};

class ScalableIphBrowserTestVersionNumberNoValue
    : public ScalableIphBrowserTest {
 protected:
  void AppendVersionNumber(base::FieldTrialParams& params) override {}
};

class ScalableIphBrowserTestVersionNumberIncorrect
    : public ScalableIphBrowserTest {
 protected:
  void AppendVersionNumber(base::FieldTrialParams& params) override {
    ScalableIphBrowserTest::AppendVersionNumber(
        params, TestIphFeature(),
        base::NumberToString(scalable_iph::kCurrentVersionNumber - 1));
  }
};

class ScalableIphBrowserTestVersionNumberInvalid
    : public ScalableIphBrowserTest {
 protected:
  void AppendVersionNumber(base::FieldTrialParams& params) override {
    ScalableIphBrowserTest::AppendVersionNumber(params, TestIphFeature(),
                                                "Invalid");
  }
};

class ScalableIphBrowserTestMultipleIphs : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params_one;
    AppendVersionNumber(params_one, TestIphFeature());
    AppendFakeUiParamsNotification(params_one, TestIphFeature());
    base::test::FeatureRefAndParams test_config_one(TestIphFeature(),
                                                    params_one);

    base::FieldTrialParams params_two;
    AppendVersionNumber(params_two, kScalableIphTestTwo);
    AppendFakeUiParamsNotification(params_two, kScalableIphTestTwo);
    base::test::FeatureRefAndParams test_config_two(kScalableIphTestTwo,
                                                    params_two);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, test_config_one, test_config_two}, {});
  }
};

class ScalableIphBrowserTestCustomConditionBase
    : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendVersionNumber(params);
    AppendFakeUiParamsNotification(params);
    AppendCustomCondition(params);
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, test_config}, {});
  }

  virtual void AppendCustomCondition(base::FieldTrialParams& params) = 0;
};

class ScalableIphBrowserTestNetworkConnection
    : public ScalableIphBrowserTestCustomConditionBase {
 protected:
  void AppendCustomCondition(base::FieldTrialParams& params) override {
    params[FullyQualified(
        TestIphFeature(),
        scalable_iph::kCustomConditionNetworkConnectionParamName)] =
        scalable_iph::kCustomConditionNetworkConnectionOnline;
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

class ScalableIphBrowserTestClientAgeBase
    : public ScalableIphBrowserTestCustomConditionBase {
 protected:
  void AppendCustomCondition(base::FieldTrialParams& params) override {
    params[FullyQualified(
        TestIphFeature(),
        scalable_iph::kCustomConditionClientAgeInDaysParamName)] =
        GetClientAgeTestValue();
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

class ScalableIphBrowserTestHasSavedPrinters
    : public ScalableIphBrowserTestCustomConditionBase {
 protected:
  void AppendCustomCondition(base::FieldTrialParams& params) override {
    params[FullyQualified(
        TestIphFeature(),
        scalable_iph::kCustomConditionHasSavedPrintersParamName)] =
        scalable_iph::kCustomConditionHasSavedPrintersValueFalse;
  }
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
 public:
  void SetUp() override {
    // Set animation duration to zero so the nudge dismisses immediately when
    // cancelled or timed out.
    ui::ScopedAnimationDurationScaleMode duration_scale(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    ScalableIphBrowserTest::SetUp();
  }

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

class ScalableIphBrowserTestNotificationInvalidConfig
    : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendVersionNumber(params);
    AppendFakeUiParamsNotification(params);
    params[FullyQualified(TestIphFeature(),
                          scalable_iph::kCustomNotificationIdParamName)] = "";
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, test_config}, {});
  }
};

class ScalableIphBrowserTestBubbleInvalidConfig
    : public ScalableIphBrowserTest {
 protected:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendVersionNumber(params);
    AppendFakeUiParamsBubble(params);
    params[FullyQualified(TestIphFeature(),
                          scalable_iph::kCustomBubbleIdParamName)] = "";
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

// Logging feature is on by default in `ScalableIphBrowserTest`.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, Log) {
  constexpr char kTestFileNamePattern[] = "*scalable_iph_browsertest.cc*";

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  CHECK(scalable_iph);

  // `logging::SetLogMessageHandler` takes a function pointer. Use a static
  // variable as a captureless lambda can be converted to a function pointer.
  static base::NoDestructor<std::vector<std::string>> captured_logs;
  CHECK_EQ(nullptr, logging::GetLogMessageHandler());
  logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                   size_t message_start,
                                   const std::string& str) {
    captured_logs->push_back(str);
    return true;
  });

  SCALABLE_IPH_LOG(scalable_iph->logger()) << kTestLogMessage;

  logging::SetLogMessageHandler(nullptr);

  EXPECT_TRUE(base::MatchPattern(scalable_iph->logger()->GenerateLog(),
                                 kTestLogMessagePattern));
  EXPECT_TRUE(base::MatchPattern(scalable_iph->logger()->GenerateLog(),
                                 kTestFileNamePattern));

  std::string log_output = base::JoinString(*captured_logs, "");
  if (DCHECK_IS_ON()) {
    EXPECT_TRUE(base::MatchPattern(log_output, kTestLogMessagePattern));
  } else {
    EXPECT_FALSE(base::MatchPattern(log_output, kTestLogMessagePattern));
  }

  // Confirms that the debug page is accessible.
  content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
      browser(), GURL(kScalableIphDebugLogTextUrl));
  ASSERT_TRUE(render_frame_host);
  ASSERT_TRUE(render_frame_host->GetLastResponseHeaders());
  EXPECT_EQ(200, render_frame_host->GetLastResponseHeaders()->response_code());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestDebugOff, NoLog) {
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  CHECK(scalable_iph);

  // `logging::SetLogMessageHandler` takes a function pointer. Use a static
  // variable as a captureless lambda can be converted to a function pointer.
  static base::NoDestructor<std::vector<std::string>> captured_logs;
  CHECK_EQ(nullptr, logging::GetLogMessageHandler());
  logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                   size_t message_start,
                                   const std::string& str) {
    captured_logs->push_back(str);
    return true;
  });

  SCALABLE_IPH_LOG(scalable_iph->logger()) << kTestLogMessage;

  logging::SetLogMessageHandler(nullptr);

  EXPECT_TRUE(scalable_iph->logger()->IsLogEmptyForTesting());

  std::string log_output = base::JoinString(*captured_logs, "");
  EXPECT_FALSE(base::MatchPattern(log_output, kTestLogMessagePattern));

  // Confirms that the debug page is not accessible if the flag is off.
  content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
      browser(), GURL(kScalableIphDebugLogTextUrl));
  ASSERT_TRUE(render_frame_host);
  // Last response headers is nullptr if there is no response. See the comment
  // of `RenderFrameHost::GetLastResponseHeaders` for details.
  EXPECT_FALSE(render_frame_host->GetLastResponseHeaders());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestMultipleIphs, OneIphAtATime) {
  EnableTestIphFeatures({&TestIphFeature(), &kScalableIphTestTwo});

  // Expects that `ShowNotification` gets called exactly once as we expect that
  // only a single IPH gets triggered at a time.
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestPreinstallApps,
                       AppListItemActivationWebApp) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP()
        << "Google Chrome is required for preinstall apps used by this test";
    return;
  }

  // Those constants in `scalable_iph` must be synced with ones in `web_app`.
  // Test them in this test case.
  EXPECT_EQ(std::string(scalable_iph::kWebAppYouTubeAppId),
            std::string(web_app::kYoutubeAppId));
  EXPECT_EQ(std::string(scalable_iph::kWebAppGoogleDocsAppId),
            std::string(web_app::kGoogleDocsAppId));

  AppListClientImpl* app_list_client_impl = AppListClientImpl::GetInstance();
  AppListModelUpdater* app_list_model_updater =
      test::GetModelUpdater(app_list_client_impl);

  AppListItemWaiter app_list_item_waiter(web_app::kYoutubeAppId,
                                         app_list_model_updater);
  app_list_item_waiter.Wait();

  app_list_client_impl->ShowAppList(ash::AppListShowSource::kSearchKey);

  EXPECT_CALL(
      *mock_tracker(),
      NotifyEvent(scalable_iph::kEventNameAppListItemActivationYouTube));
  app_list_client_impl->ActivateItem(
      /*profile_id=*/0, web_app::kYoutubeAppId, /*event_flags=*/0,
      ash::AppListLaunchedFrom::kLaunchedFromGrid);
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestOobe, SessionState) {
  EnableTestIphFeature();

  // Confirm that no trigger condition check should happen during OOBE.
  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(0);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Confirm that a trigger condition check happens immediately after OOBE.
  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(1);
  ash::WizardController::default_controller()->SkipPostLoginScreensForTesting();
  GetLoginManagerMixin()->WaitForActiveSession();
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
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

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestHasSavedPrinters,
                       ExpectNoSavedPrinters) {
  EnableTestIphFeature();

  constexpr char kTestPrinterId[] = "test-printer-id";

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  CHECK(scalable_iph);

  ash::SyncedPrintersManager* synced_printers_manager =
      ash::SyncedPrintersManagerFactory::GetForBrowserContext(
          browser()->profile());
  CHECK(synced_printers_manager);

  // Add a printer. Expect that no IPH gets triggered if there is a saved
  // printer.
  {
    base::RunLoop run_loop;
    scalable_iph->SetHasSavedPrintersChangedClosureForTesting(
        run_loop.QuitClosure());
    synced_printers_manager->UpdateSavedPrinter(
        chromeos::Printer(kTestPrinterId));
    run_loop.Run();
  }

  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // Remove the printer and confirm that an IPH gets triggered.
  {
    base::RunLoop run_loop;
    scalable_iph->SetHasSavedPrintersChangedClosureForTesting(
        run_loop.QuitClosure());
    synced_printers_manager->RemoveSavedPrinter(kTestPrinterId);
    run_loop.Run();
  }

  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
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

  // `PerformActionForScalableIph` should be called with the corresponding CTA
  // action_type when a notification is clicked.
  EXPECT_CALL(*mock_delegate(), PerformActionForScalableIph(::testing::Eq(
                                    scalable_iph::ActionType::kOpenChrome)));
  notification->delegate()->Click(/*button_index=*/0, /*reply=*/absl::nullopt);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, InvokeIphByTimer_Bubble) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  scalable_iph::ScalableIphDelegate::BubbleParams expected_params;
  expected_params.bubble_id = ScalableIphBrowserTestBase::kTestBubbleId;
  expected_params.title = ScalableIphBrowserTestBase::kTestBubbleTitle;
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
  expected_params.title = ScalableIphBrowserTestBase::kTestBubbleTitle;
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

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, ShowBubbleAndDismiss) {
  EnableTestIphFeature();
  mock_delegate()->FakeShowBubble();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is not performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent)).Times(0);

  {
    // A timer used for a nudge dismiss is created via
    // `AnchoredNudgeManager::Show` call. Call the method in the scoped context
    // of `TestMockTimeTaskRunner` as we can fast-forward it below.
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner());
    TriggerConditionsCheckWithAFakeEvent(
        scalable_iph::ScalableIph::Event::kFiveMinTick);
  }

  ash::AnchoredNudgeManager* anchored_nudge_manager =
      ash::AnchoredNudgeManager::Get();
  CHECK(anchored_nudge_manager);
  EXPECT_TRUE(anchored_nudge_manager->IsNudgeShown(kTestBubbleId));

  // Fast forward nudge medium duration + 1 second.
  task_runner()->FastForwardBy(
      ash::AnchoredNudgeManagerImpl::kNudgeMediumDuration + base::Seconds(1));

  EXPECT_FALSE(anchored_nudge_manager->IsNudgeShown(kTestBubbleId));

  testing::Mock::VerifyAndClearExpectations(mock_tracker());
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
  EXPECT_CALL(*mock_delegate(), PerformActionForScalableIph(::testing::Eq(
                                    scalable_iph::ActionType::kOpenGoogleDocs)))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  ash::AnchoredNudgeManager* anchored_nudge_manager =
      ash::AnchoredNudgeManager::Get();
  CHECK(anchored_nudge_manager);
  EXPECT_TRUE(anchored_nudge_manager->IsNudgeShown(kTestBubbleId));

  ash::AnchoredNudgeManager::Get()->Cancel(kTestBubbleId);
  EXPECT_FALSE(anchored_nudge_manager->IsNudgeShown(kTestBubbleId));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubble, ClickBubble) {
  EnableTestIphFeature();
  mock_delegate()->FakeShowBubble();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent)).Times(1);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  ash::AnchoredNudgeManager* anchored_nudge_manager =
      ash::AnchoredNudgeManager::Get();
  CHECK(anchored_nudge_manager);
  EXPECT_TRUE(anchored_nudge_manager->IsNudgeShown(kTestBubbleId));

  // `PerformActionForScalableIph` should be called with the corresponding CTA
  // action_type when a bubble is clicked.
  EXPECT_CALL(*mock_delegate(),
              PerformActionForScalableIph(
                  ::testing::Eq(scalable_iph::ActionType::kOpenGoogleDocs)));

  views::View* nudge_button =
      ash::Shell::Get()->anchored_nudge_manager()->GetNudgeFirstButtonForTest(
          kTestBubbleId);
  ui::test::EventGenerator event_generator(ash::Shell::GetPrimaryRootWindow());
  event_generator.MoveMouseTo(nudge_button->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  EXPECT_FALSE(anchored_nudge_manager->IsNudgeShown(kTestBubbleId));
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNotificationInvalidConfig,
                       NotShowNotification) {
  EnableTestIphFeature();

  // Tracker::Dismissed must be called when an IPH gets dismissed.
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is not performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent)).Times(0);

  // Simulate an invalid config (i.e. missing notification_id).
  scalable_iph::ScalableIphDelegate::NotificationParams invalid_params;
  invalid_params.notification_id = "";
  invalid_params.title = ScalableIphBrowserTestBase::kTestNotificationTitle;
  invalid_params.text = ScalableIphBrowserTestBase::kTestNotificationBodyText;
  invalid_params.button.text =
      ScalableIphBrowserTestBase::kTestNotificationButtonText;
  invalid_params.button.action.action_type =
      scalable_iph::ActionType::kOpenChrome;
  invalid_params.button.action.iph_event_name =
      ScalableIphBrowserTestBase::kTestButtonActionEvent;

  // When the config params are invalid and/or not parsable, the notification
  // should not be shown.
  EXPECT_CALL(*mock_delegate(), ShowNotification(::testing::Eq(invalid_params),
                                                 ::testing::NotNull()))
      .Times(0);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  // Check that a notification is not shown.
  auto* message_center = message_center::MessageCenter::Get();
  auto* notification = message_center->FindVisibleNotificationById("");
  EXPECT_FALSE(notification);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestBubbleInvalidConfig,
                       NotShowBubble) {
  EnableTestIphFeature();
  mock_delegate()->FakeShowBubble();

  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameFiveMinTick));
  // The action is not performed.
  EXPECT_CALL(*mock_tracker(), NotifyEvent(kTestButtonActionEvent)).Times(0);

  // Simulate an invalid config (i.e. missing bubble_id).
  scalable_iph::ScalableIphDelegate::BubbleParams invalid_params;
  invalid_params.bubble_id = "";
  invalid_params.title = ScalableIphBrowserTestBase::kTestBubbleTitle;
  invalid_params.text = ScalableIphBrowserTestBase::kTestBubbleText;
  invalid_params.button.text =
      ScalableIphBrowserTestBase::kTestBubbleButtonText;
  invalid_params.button.action.action_type =
      scalable_iph::ActionType::kOpenGoogleDocs;
  invalid_params.button.action.iph_event_name =
      ScalableIphBrowserTestBase::kTestButtonActionEvent;
  invalid_params.icon =
      scalable_iph::ScalableIphDelegate::BubbleIcon::kGoogleDocsIcon;

  // When the config params are invalid and/or not parsable, the notification
  // should not be shown.
  EXPECT_CALL(*mock_delegate(),
              ShowBubble(::testing::Eq(invalid_params), ::testing::NotNull()))
      .Times(0);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  ash::AnchoredNudgeManager* anchored_nudge_manager =
      ash::AnchoredNudgeManager::Get();
  CHECK(anchored_nudge_manager);
  EXPECT_FALSE(anchored_nudge_manager->IsNudgeShown(""));
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
