// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/test_widget_builder.h"
#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "base/strings/pattern.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/account_id/account_id.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

using ScalableIphBrowserTest = ::ash::ScalableIphBrowserTestBase;
using TestEnvironment =
    ::ash::CustomizableTestEnvBrowserTestBase::TestEnvironment;
using UserSessionType =
    ::ash::CustomizableTestEnvBrowserTestBase::UserSessionType;

constexpr char kTestLogMessage[] = "test-log-message";
constexpr char kTestLogMessagePattern[] = "*test-log-message*";
constexpr char kScalableIphDebugLogTextUrl[] =
    "chrome-untrusted://scalable-iph-debug/log.txt";
constexpr char16_t kTestGameWindowTitle[] = u"ScalableIphTestGameWindow";

BASE_FEATURE(kScalableIphTestTwo,
             "ScalableIphTestTwo",
             base::FEATURE_DISABLED_BY_DEFAULT);

void OverrideStoredPermanentCountry(std::string_view country_code) {
  CHECK(g_browser_process->variations_service()->OverrideStoredPermanentCountry(
      std::string(country_code)));
}

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

std::unique_ptr<aura::Window> CreateAuraWindow(std::u16string window_title) {
  ash::TestWidgetBuilder builder;
  builder.SetWindowTitle(window_title);
  builder.SetTestWidgetDelegate();
  builder.SetContext(ash::Shell::GetPrimaryRootWindow());
  builder.SetBounds(gfx::Rect(0, 0, 600, 400));
  views::Widget* widget = builder.BuildOwnedByNativeWidget();
  return std::unique_ptr<aura::Window>(widget->GetNativeWindow());
}

class IsOnlineValueWaiter {
 public:
  IsOnlineValueWaiter(scalable_iph::ScalableIphDelegate* scalable_iph_delegate,
                      bool expected)
      : scalable_iph_delegate_(scalable_iph_delegate), expected_(expected) {}

  void Wait() {
    if (scalable_iph_delegate_->IsOnline() == expected_) {
      return;
    }

    repeating_timer_.Start(FROM_HERE, base::Milliseconds(10),
                           base::BindRepeating(&IsOnlineValueWaiter::Check,
                                               weak_ptr_factory_.GetWeakPtr()));
    run_loop_.Run();
  }

 private:
  void Check() {
    if (scalable_iph_delegate_->IsOnline() != expected_) {
      return;
    }

    run_loop_.Quit();
  }

  raw_ptr<scalable_iph::ScalableIphDelegate> scalable_iph_delegate_;
  bool expected_;
  base::RepeatingTimer repeating_timer_;
  base::RunLoop run_loop_;
  base::WeakPtrFactory<IsOnlineValueWaiter> weak_ptr_factory_{this};
};

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

class CupsPrintJobManagerWaiter : public ash::CupsPrintJobManager::Observer {
 public:
  CupsPrintJobManagerWaiter(ash::CupsPrintJobManager* print_job_manager,
                            int job_id)
      : print_job_manager_(print_job_manager), job_id_(job_id) {
    CHECK(print_job_manager_);
    print_job_manager_observation_.Observe(print_job_manager_);
  }

  void Wait() { run_loop_.Run(); }

  void OnPrintJobCreated(base::WeakPtr<ash::CupsPrintJob> job) override {
    if (job->job_id() == job_id_) {
      CHECK(run_loop_.IsRunningOnCurrentThread())
          << "Observed expected print job id before run_loop_ is running: "
          << job_id_;
      run_loop_.Quit();
    }
  }

 private:
  base::ScopedObservation<ash::CupsPrintJobManager,
                          ash::CupsPrintJobManager::Observer>
      print_job_manager_observation_{this};
  raw_ptr<ash::CupsPrintJobManager> print_job_manager_;
  base::RunLoop run_loop_;
  int job_id_;
};

class ScalableIphBrowserTestFlagOff
    : public ash::CustomizableTestEnvBrowserTestBase {
 public:
  ScalableIphBrowserTestFlagOff() {
    scoped_feature_list_.InitAndDisableFeature(ash::features::kScalableIph);
    scalable_iph::ScalableIph::ForceEnableIphFeatureForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ScalableIphBrowserTestNoIph
    : public ash::CustomizableTestEnvBrowserTestBase {
 public:
  ScalableIphBrowserTestNoIph() {
    // Disable all IPH feature flags to avoid one of them get enabled by
    // fieldtrial testing config. Fieldtrial testing config won't flip a
    // flag if it's already force-enabled/disabled. Convert it to a vector
    // of `FeatureRef`. `ScalableIph` cannot depend on it as it's in base::test.
    std::vector<base::test::FeatureRef> disabled_features_refs;
    const std::vector<raw_ptr<const base::Feature, VectorExperimental>>&
        disabled_features =
            scalable_iph::ScalableIph::GetFeatureListConstantForTesting();
    for (auto feature : disabled_features) {
      disabled_features_refs.push_back(base::test::FeatureRef(*feature));
    }

    scoped_feature_list_.InitWithFeatures({ash::features::kScalableIph},
                                          disabled_features_refs);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ScalableIphBrowserTestGame : public ScalableIphBrowserTest {
 public:
  void AppendTestSpecificFeatures(
      std::vector<base::test::FeatureRefAndParams>& enabled_features,
      std::vector<base::test::FeatureRef>& disabled_features) override {
    enabled_features.push_back(
        base::test::FeatureRefAndParams(ash::features::kGameDashboard, {}));
  }

  void SetUpOnMainThread() override {
    ScalableIphBrowserTest::SetUpOnMainThread();

    CHECK(ash::GameDashboardController::Get())
        << "Game dashboard has to be enabled for this test.";
  }
};

class ScalableIphBrowserTestGameMultiUser : public ScalableIphBrowserTestGame {
 public:
  ScalableIphBrowserTestGameMultiUser() { enable_multi_user_ = true; }
};

class ScalableIphBrowserTestDebugOff : public ScalableIphBrowserTest {
 public:
  ScalableIphBrowserTestDebugOff() { enable_scalable_iph_debug_ = false; }
};

class ScalableIphBrowserTestFeatureOffDebugOn : public ScalableIphBrowserTest {
 public:
  ScalableIphBrowserTestFeatureOffDebugOn() {
    enable_scalable_iph_ = false;
    setup_scalable_iph_ = false;
    CHECK(enable_scalable_iph_debug_)
        << "Debug feature is on by default for ScalableIphBrowserTest";
  }
};

class ScalableIphBrowserTestPreinstallApps : public ScalableIphBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    ScalableIphBrowserTest::SetUpDefaultCommandLine(command_line);

    command_line->RemoveSwitch(switches::kDisableDefaultApps);
    command_line->AppendSwitch(
        ash::switches::kAllowDefaultShelfPinLayoutIgnoringSync);
  }
};

class ScalableIphBrowserTestHelpApp
    : public ScalableIphBrowserTestPreinstallApps {
 public:
  void InitializeScopedFeatureList() override {
    base::FieldTrialParams params;
    AppendVersionNumber(params);
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});

    base::test::FeatureRefAndParams help_app_feature(
        ash::features::kHelpAppWelcomeTips, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, help_app_feature, test_config}, {});
  }

  void SetUpOnMainThread() override {
    ScalableIphBrowserTestPreinstallApps::SetUpOnMainThread();

    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
  }
};

class ScalableIphBrowserTestHelpAppParameterized
    : public ScalableIphBrowserTestHelpApp,
      public testing::WithParamInterface<TestEnvironment> {
 public:
  void SetUp() override {
    SetTestEnvironment(GetParam());
    setup_scalable_iph_ = false;

    ScalableIphBrowserTestHelpApp::SetUp();
  }
};

class ScalableIphBrowserTestPerksMinecraftRealms
    : public ScalableIphBrowserTest {
 protected:
  void AppendUiParams(base::FieldTrialParams& params) override {
    ScalableIphBrowserTest::AppendFakeUiParamsNotification(
        params, /*has_body_text=*/false, TestIphFeature());
    params[FullyQualified(TestIphFeature(),
                          scalable_iph::kCustomButtonActionTypeParamName)] =
        scalable_iph::kActionTypeOpenChromebookPerksMinecraftRealms2023;
  }
};

class PerksEnvironment {
 public:
  PerksEnvironment(std::string_view country_code, const GURL& perks_url)
      : country_code_(country_code), perks_url_(perks_url) {}

  const std::string& country_code() const { return country_code_; }

  GURL perks_url() const { return perks_url_; }

  static std::string GenerateTestName(
      testing::TestParamInfo<PerksEnvironment> test_param_info) {
    const PerksEnvironment& param = test_param_info.param;
    return param.country_code();
  }

 private:
  const std::string country_code_;
  const GURL perks_url_;
};

class ScalableIphBrowserTestPerksMinecraftRealmsParameterized
    : public ScalableIphBrowserTestPerksMinecraftRealms,
      public testing::WithParamInterface<PerksEnvironment> {};

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
    AppendFakeUiParamsNotification(params_one,
                                   /*has_body_text=*/true, TestIphFeature());
    base::test::FeatureRefAndParams test_config_one(TestIphFeature(),
                                                    params_one);

    base::FieldTrialParams params_two;
    AppendVersionNumber(params_two, kScalableIphTestTwo);
    AppendFakeUiParamsNotification(params_two,
                                   /*has_body_text=*/true, kScalableIphTestTwo);
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
    AppendUiParams(params);
    AppendCustomCondition(params);
    base::test::FeatureRefAndParams test_config(TestIphFeature(), params);

    base::test::FeatureRefAndParams scalable_iph_feature(
        ash::features::kScalableIph, {});
    base::test::FeatureRefAndParams scalable_iph_debug_feature(
        ash::features::kScalableIphDebug, {});

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {scalable_iph_feature, scalable_iph_debug_feature, test_config}, {});
  }

  virtual void AppendCustomCondition(base::FieldTrialParams& params) = 0;
};

class ScalableIphBrowserTestTriggerEvent
    : public ScalableIphBrowserTestCustomConditionBase {
 protected:
  void AppendCustomCondition(base::FieldTrialParams& params) override {
    params[FullyQualified(
        TestIphFeature(),
        scalable_iph::kCustomConditionTriggerEventParamName)] =
        scalable_iph::kEventNameUnlocked;
  }
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

    // There is an async call for querying network status.
    ash::ScalableIphDelegateImpl* scalable_iph_delegate_impl =
        static_cast<ash::ScalableIphDelegateImpl*>(
            mock_delegate()->fake_delegate());
    IsOnlineValueWaiter is_online_value_waiter(scalable_iph_delegate_impl,
                                               true);
    is_online_value_waiter.Wait();
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

class ScalableIphBrowserTestPhoneHubOnboardingEligible
    : public ScalableIphBrowserTestCustomConditionBase {
 public:
  void SetUpOnMainThread() override {
    ScalableIphBrowserTestCustomConditionBase::SetUpOnMainThread();
    ash::ScalableIphDelegateImpl* scalable_iph_delegate_impl =
        static_cast<ash::ScalableIphDelegateImpl*>(
            mock_delegate()->fake_delegate());

    scalable_iph_delegate_impl->SetFakeFeatureStatusProviderForTesting(
        &fake_feature_status_provider_);
  }

 protected:
  void AppendCustomCondition(base::FieldTrialParams& params) override {
    params[FullyQualified(
        TestIphFeature(),
        scalable_iph::kCustomConditionPhoneHubOnboardingEligibleParamName)] =
        scalable_iph::kCustomConditionPhoneHubOnboardingEligibleValueTrue;
  }

  ash::phonehub::FakeFeatureStatusProvider fake_feature_status_provider_;
};

class ScalableIphBrowserTestParameterized
    : public ScalableIphBrowserTest,
      public testing::WithParamInterface<TestEnvironment> {
 public:
  ScalableIphBrowserTestParameterized() {
    // Set `false` as `ScalableIphBrowserTestParameterized` is used to test
    // ScalableIph is not eligible cases.
    setup_scalable_iph_ = false;
  }

  void SetUp() override {
    SetTestEnvironment(GetParam());

    ash::CustomizableTestEnvBrowserTestBase::SetUp();
  }
};

class ScalableIphBrowserTestMinor : public ScalableIphBrowserTest {
 public:
  ScalableIphBrowserTestMinor() {
    // `ScalableIphFactoryImpl::GetBrowserContextToUseInternal` uses manta
    // service eligibility as a signal to see if a user is a minor or not. Force
    // disable manta service to simulate minor user case.
    force_disable_manta_service_ = true;

    setup_scalable_iph_ = false;
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
 public:
  ScalableIphBrowserTestNotification() : has_body_text_(true) {}

 protected:
  explicit ScalableIphBrowserTestNotification(bool has_body_text)
      : has_body_text_(has_body_text) {}

  void SetUpOnMainThread() override {
    ScalableIphBrowserTest::SetUpOnMainThread();

    auto* message_center = message_center::MessageCenter::Get();
    scoped_observation_.Observe(message_center);
    EXPECT_CALL(mock_, OnNotificationAdded(kTestNotificationId));

    mock_delegate()->FakeShowNotification();
  }

  void AppendUiParams(base::FieldTrialParams& params) override {
    AppendFakeUiParamsNotification(params, has_body_text_, TestIphFeature());
  }

  void TearDownOnMainThread() override {
    scoped_observation_.Reset();

    ScalableIphBrowserTest::TearDownOnMainThread();
  }

 protected:
  const bool has_body_text_;

 private:
  // Observe notifications.
  MockMessageCenterObserver mock_;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      scoped_observation_{&mock_};
};

class ScalableIphBrowserTestNotificationNoBodyText
    : public ScalableIphBrowserTestNotification {
 public:
  ScalableIphBrowserTestNotificationNoBodyText()
      : ScalableIphBrowserTestNotification(/*has_body_text=*/false) {}
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
    AppendFakeUiParamsNotification(params, /*has_body_text=*/true,
                                   TestIphFeature());
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

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestFlagOff, ScalableIphOff) {
  ASSERT_FALSE(ash::features::IsScalableIphEnabled());
  ASSERT_TRUE(scalable_iph::ScalableIph::IsAnyIphFeatureEnabled());

  EXPECT_FALSE(ScalableIphFactory::GetForBrowserContext(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNoIph, NoIphFeatureFlagOn) {
  ASSERT_TRUE(ash::features::IsScalableIphEnabled());
  ASSERT_FALSE(scalable_iph::ScalableIph::IsAnyIphFeatureEnabled());

  EXPECT_FALSE(ScalableIphFactory::GetForBrowserContext(browser()->profile()));
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
        return true;
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
        return true;
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

// TODO(crbug.com/40924957): Flaky test.
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

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class ScalableIphBrowserTestNoTestingConfig : public ScalableIphBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ScalableIphBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNoTestingConfig, AppListShown) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameAppListShown));

  ash::AppListTestApi().ShowBubbleAppListAndWait();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, OpenPersonalizationApp) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameOpenPersonalizationApp));

  ash::LaunchSystemWebAppAsync(browser()->profile(),
                               ash::SystemWebAppType::PERSONALIZATION);
}

// TODO(b/301006258): Migrate to use observer pattern, then enable the test.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTest, DISABLED_PrintJobCreated) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNamePrintJobCreated));

  ash::CupsPrintJobManager* print_job_manager =
      ash::CupsPrintJobManagerFactory::GetForBrowserContext(
          browser()->profile());
  CupsPrintJobManagerWaiter print_job_manager_waiter(print_job_manager,
                                                     /*job_id=*/0);
  print_job_manager->CreatePrintJob(
      "test-printer-id", "title", /*job_id=*/0, /*total_page_number=*/1,
      ::printing::PrintJob::Source::kPrintPreview, /*source_id=*/"",
      ash::printing::proto::PrintSettings());
  print_job_manager_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestGame, GameWindowOpened) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameGameWindowOpened));

  std::unique_ptr<aura::Window> window = CreateAuraWindow(kTestGameWindowTitle);
  window->SetProperty(ash::kAppIDKey,
                      std::string(extension_misc::kGeForceNowAppId));
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestGameMultiUser,
                       NoGameWindowOpenedForSecondaryUser) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameGameWindowOpened))
      .Times(0);

  // Login with secondary user and open a game window with the user.
  ash::UserAddingScreen::Get()->Start();
  CHECK(GetLoginManagerMixin()->LoginAndWaitForActiveSession(
      GetSecondaryUserContext()));

  std::unique_ptr<aura::Window> window = CreateAuraWindow(kTestGameWindowTitle);
  window->SetProperty(ash::kAppIDKey,
                      std::string(extension_misc::kGeForceNowAppId));
  ash::MultiUserWindowManager* multi_user_window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  CHECK(multi_user_window_manager);
  multi_user_window_manager->SetWindowOwner(
      window.get(), GetSecondaryUserContext().GetAccountId());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestGameMultiUser,
                       NoGameWindowOpenedTeleport) {
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameGameWindowOpened))
      .Times(0);

  // Login with secondary user and open a game window with the user.
  ash::UserAddingScreen::Get()->Start();
  CHECK(GetLoginManagerMixin()->LoginAndWaitForActiveSession(
      GetSecondaryUserContext()));

  std::unique_ptr<aura::Window> window = CreateAuraWindow(kTestGameWindowTitle);
  ash::MultiUserWindowManager* multi_user_window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  CHECK(multi_user_window_manager);
  multi_user_window_manager->SetWindowOwner(
      window.get(), GetSecondaryUserContext().GetAccountId());

  // Teleport the window before app id is set.
  multi_user_window_manager->ShowWindowForUser(
      window.get(), GetPrimaryUserContext().GetAccountId());

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  CHECK(user_manager);
  CHECK_EQ(user_manager->GetActiveUser()->GetAccountId(),
           GetPrimaryUserContext().GetAccountId());

  window->SetProperty(ash::kAppIDKey,
                      std::string(extension_misc::kGeForceNowAppId));
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

  SCALABLE_IPH_LOG(scalable_iph->GetLogger()) << kTestLogMessage;

  logging::SetLogMessageHandler(nullptr);

  EXPECT_TRUE(base::MatchPattern(scalable_iph->GetLogger()->GenerateLog(),
                                 kTestLogMessagePattern));
  EXPECT_TRUE(base::MatchPattern(scalable_iph->GetLogger()->GenerateLog(),
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
  const network::mojom::URLResponseHead* head =
      render_frame_host->GetLastResponseHead();
  ASSERT_TRUE(head);
  ASSERT_TRUE(head->headers);
  EXPECT_EQ(net::HTTP_OK, head->headers->response_code());
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

  SCALABLE_IPH_LOG(scalable_iph->GetLogger()) << kTestLogMessage;

  logging::SetLogMessageHandler(nullptr);

  EXPECT_TRUE(scalable_iph->GetLogger()->IsLogEmptyForTesting());

  std::string log_output = base::JoinString(*captured_logs, "");
  EXPECT_FALSE(base::MatchPattern(log_output, kTestLogMessagePattern));

  // Confirms that the debug page is not accessible if the flag is off.
  content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
      browser(), GURL(kScalableIphDebugLogTextUrl));
  ASSERT_TRUE(render_frame_host);
  // Last response head is nullptr if there is no response. See the comment
  // of `RenderFrameHost::GetLastResponseHead` for details.
  EXPECT_FALSE(render_frame_host->GetLastResponseHead());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestFeatureOffDebugOn,
                       LogPageAvailable) {
  content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
      browser(), GURL(kScalableIphDebugLogTextUrl));
  ASSERT_TRUE(render_frame_host);
  const network::mojom::URLResponseHead* head =
      render_frame_host->GetLastResponseHead();
  ASSERT_TRUE(head);
  ASSERT_TRUE(head->headers);
  EXPECT_EQ(net::HTTP_OK, head->headers->response_code())
      << "Debug log page is expected to be available even if ScalableIph "
         "feature itself is off.";
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestMultipleIphs, OneIphAtATime) {
  EnableTestIphFeatures({&TestIphFeature(), &kScalableIphTestTwo});

  // Expects that `ShowNotification` gets called exactly once as we expect that
  // only a single IPH gets triggered at a time.
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .WillOnce(testing::Return(true));
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestPreinstallApps,
                       AppListItemActivationWebApp) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP()
        << "Google Chrome is required for preinstall apps used by this test";
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

  ash::AppListTestApi().ShowBubbleAppListAndWait();

  EXPECT_CALL(
      *mock_tracker(),
      NotifyEvent(scalable_iph::kEventNameAppListItemActivationYouTube));
  app_list_client_impl->ActivateItem(
      /*profile_id=*/0, web_app::kYoutubeAppId, /*event_flags=*/0,
      ash::AppListLaunchedFrom::kLaunchedFromGrid, /*is_above_the_fold=*/true);
}

// TODO(crbug.com/328713274): Test is flaky.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestPreinstallApps,
                       DISABLED_ShelfItemActivationWebApp) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP()
        << "Google Chrome is required for preinstall apps used by this test";
  }

  apps::AppReadinessWaiter(browser()->profile(),
                           scalable_iph::kWebAppYouTubeAppId)
      .Await();

  EXPECT_CALL(*mock_tracker(),
              NotifyEvent(scalable_iph::kEventNameShelfItemActivationYouTube));
  ash::Shelf::ActivateShelfItem(ash::ShelfModel::Get()->ItemIndexByAppID(
      scalable_iph::kWebAppYouTubeAppId));
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestHelpApp, HelpAppPinnedToShelf) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP()
        << "Google Chrome is required for preinstall apps used by this test";
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->IsAppPinned(web_app::kHelpAppId));
}

INSTANTIATE_TEST_SUITE_P(
    Perks,
    ScalableIphBrowserTestPerksMinecraftRealmsParameterized,
    testing::Values(
        PerksEnvironment("us",
                         GURL("https://www.google.com/chromebook/perks/"
                              "?id=minecraft.realms.2023")),
        PerksEnvironment("gb",
                         GURL("https://www.google.com/chromebook/perks/"
                              "?id=minecraft.uk.2023")),
        PerksEnvironment("ca",
                         GURL("https://www.google.com/chromebook/perks/"
                              "?id=minecraft.realms.ca.2023")),
        PerksEnvironment("au",
                         GURL("https://www.google.com/chromebook/perks/"
                              "?id=minecraft.realms.au.2023"))),
    &PerksEnvironment::GenerateTestName);

IN_PROC_BROWSER_TEST_P(ScalableIphBrowserTestPerksMinecraftRealmsParameterized,
                       Config) {
  const std::string country_code = GetParam().country_code();
  const GURL expected_perks_url = GetParam().perks_url();

  EnableTestIphFeature();

  mock_delegate()->FakeShowNotification();
  mock_delegate()->FakePerformActionForScalableIph();
  OverrideStoredPermanentCountry(country_code);

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center::Notification* notification =
      message_center->FindVisibleNotificationById(kTestNotificationId);
  ASSERT_TRUE(notification);
  ASSERT_TRUE(notification->delegate());

  ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;
  notification->delegate()->Click(/*button_index=*/0, /*reply=*/std::nullopt);
  content::WebContents* web_contents = tab_added_waiter.Wait();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(expected_perks_url, web_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestPerksMinecraftRealms,
                       ActionNotEligible) {
  const std::string country_code_jp("jp");

  EnableTestIphFeature();

  mock_delegate()->FakeShowNotification();
  mock_delegate()->FakePerformActionForScalableIph();
  OverrideStoredPermanentCountry(country_code_jp);

  // If an action is not eligible for a context, IPH is considered to be
  // dismissed immediately.
  EXPECT_CALL(*mock_tracker(), Dismissed(::testing::Ref(TestIphFeature())));

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center::Notification* notification =
      message_center->FindVisibleNotificationById(kTestNotificationId);
  EXPECT_FALSE(notification)
      << scalable_iph::kActionTypeOpenChromebookPerksMinecraftRealms2023
      << " is not supported in " << country_code_jp
      << ". No notification is expected as this config is considered to be "
         "invalid.";
}

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestOobe, SessionState) {
  // This is testing post login OOBE screens. In post login OOBE screens:
  // - A profile should be loaded.
  // - Session state should be OOBE.
  // - `ScalableIph` should not have been initialized yet.
  ASSERT_EQ(ash::Shell::Get()->session_controller()->GetSessionState(),
            session_manager::SessionState::OOBE)
      << "This is an assertion for the test framework. Session state must be "
         "OOBE during post-login OOBE";

  EXPECT_EQ(nullptr, ScalableIphFactory::GetForBrowserContext(
                         ProfileManager::GetActiveUserProfile()));
  ASSERT_EQ(nullptr, mock_delegate());

  // Complete post login OOBE screens. With the completion:
  // - Session state will transit from `OOBE` to `LOGGED_IN_NOT_ACTIVE`, and
  //   then `ACTIVE`.
  // - `ScalableIph` should be initialized.
  ash::WizardController::default_controller()->SkipPostLoginScreensForTesting();
  GetLoginManagerMixin()->WaitForActiveSession();
  ASSERT_EQ(ash::Shell::Get()->session_controller()->GetSessionState(),
            session_manager::SessionState::ACTIVE)
      << "This is an assertion for the test framework. Session state must be "
         "ACTIVE after post-login OOBE";

  EXPECT_NE(nullptr, ScalableIphFactory::GetForBrowserContext(
                         ProfileManager::GetActiveUserProfile()));
  SetUpMocks();
  EnableTestIphFeature();
  ASSERT_TRUE(mock_delegate());

  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(1);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
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

// `ScalableIphBrowserTestTriggerEvent` is set up with
// x_CustomConditionTriggerEvent: ScalableIphUnlocked.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestTriggerEvent, TriggerEvent) {
  EnableTestIphFeature();

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());

  // Record an uninterested event. Confirm that this won't trigger an IPH
  // trigger condition check.
  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(0);
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());

  // Record an unlocked event, which is an interested event. Confirm that this
  // triggers an IPH trigger condition check.
  EXPECT_CALL(*mock_tracker(),
              ShouldTriggerHelpUI(::testing::Ref(TestIphFeature())))
      .Times(1);
  scalable_iph->RecordEvent(scalable_iph::ScalableIph::Event::kUnlocked);
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

// Test config is x_CustomConditionPhoneHubOnboardingEligible: True.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestPhoneHubOnboardingEligible,
                       Eligible) {
  EnableTestIphFeature();

  // The condition should not be satisfied for `kNotEligibleForFeature`.
  fake_feature_status_provider_.SetStatus(
      ash::phonehub::FeatureStatus::kNotEligibleForFeature);
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(0);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // The condition should be satisfied for `kEligiblePhoneButNotSetUp`.
  fake_feature_status_provider_.SetStatus(
      ash::phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  EXPECT_CALL(*mock_delegate(),
              ShowNotification(::testing::_, ::testing::NotNull()))
      .Times(1);
  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // The condition should be satisfied for `kDisabled`. See the comment of
  // `FeatureStatus::kDisabled` about the meaning of the enum value.
  fake_feature_status_provider_.SetStatus(
      ash::phonehub::FeatureStatus::kDisabled);
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
  notification->delegate()->Click(/*button_index=*/0, /*reply=*/std::nullopt);
  testing::Mock::VerifyAndClearExpectations(mock_tracker());
}

// Test that a scalable_iph NotificationParam with an empty body text can create
// a notification, i.e., make sure that it's accepted input.
IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestNotificationNoBodyText,
                       ShowNotification) {
  EnableTestIphFeature();

  TriggerConditionsCheckWithAFakeEvent(
      scalable_iph::ScalableIph::Event::kFiveMinTick);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center::Notification* notification =
      message_center->FindVisibleNotificationById(kTestNotificationId);
  EXPECT_TRUE(notification);
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
            return true;
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
            return true;
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
      ash::Shell::Get()->anchored_nudge_manager()->GetNudgePrimaryButtonForTest(
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

IN_PROC_BROWSER_TEST_F(ScalableIphBrowserTestMinor, ScalableIphNotAvailable) {
  ASSERT_EQ(ash::CustomizableTestEnvBrowserTestBase::UserSessionType::kRegular,
            test_environment().user_session_type())
      << "This test uses kRegular user session type without "
         "can_use_manta_service=true capability to simulate minor account.";

  EXPECT_EQ(nullptr,
            ScalableIphFactory::GetForBrowserContext(browser()->profile()));
}

INSTANTIATE_TEST_SUITE_P(
    NoHelpAppPin,
    ScalableIphBrowserTestHelpAppParameterized,
    testing::Values(
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kGuest),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kManaged),
        TestEnvironment(
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
            UserSessionType::kRegularNonOwner)),
    &TestEnvironment::GenerateTestName);

IN_PROC_BROWSER_TEST_P(ScalableIphBrowserTestHelpAppParameterized,
                       HelpAppNotPinnedToShelf) {
  EXPECT_FALSE(ash::ShelfModel::Get()->IsAppPinned(web_app::kHelpAppId));
}
