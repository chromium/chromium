// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_chrome_activity_metrics.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {

namespace {
constexpr char kExtensionNameChrome[] = "Chrome";
constexpr char kExtensionAppUrl[] = "https://example.com/";
constexpr base::TimeDelta kHalfHour = base::Minutes(30);
constexpr base::TimeDelta kOneMinute = base::Minutes(1);

constexpr apps::InstanceState kActiveInstanceState =
    static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
constexpr apps::InstanceState kInactiveInstanceState =
    static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                     apps::InstanceState::kRunning);

void SetScreenOff(bool is_screen_off) {
  power_manager::ScreenIdleState screen_idle_state;
  screen_idle_state.set_off(is_screen_off);
  chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(
      screen_idle_state);
}

}  // namespace

class FamilyUserChromeActivityMetricsTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FamilyUserChromeActivityMetricsTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~FamilyUserChromeActivityMetricsTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ChromeRenderViewHostTestHarness::SetUp();
    InitiateFamilyUserChromeActivityMetrics();
    WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(profile()));

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        /*command_line=*/base::CommandLine::ForCurrentProcess(),
        /*install_directory=*/base::FilePath(), /*autoupdate_enabled=*/false);
    extension_service_->Init();

    // Install Chrome.
    scoped_refptr<extensions::Extension> chrome = CreateExtension(
        app_constants::kChromeAppId, kExtensionNameChrome, kExtensionAppUrl);
    extension_service_->AddComponentExtension(chrome.get());

    BrowserList* active_browser_list = BrowserList::GetInstance();
    // Expect BrowserList is empty at the beginning.
    EXPECT_EQ(0U, active_browser_list->size());
    test_browser_ = CreateBrowserWithAuraWindow();
    EXPECT_EQ(1U, active_browser_list->size());

    // Set the app active. If the app is active, it should be started, running,
    // and visible.
    PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                          kActiveInstanceState);
  }

  void TearDown() override {
    test_browser_.reset();
    DestroyFamilyUserChromeActivityMetrics();
    ChromeRenderViewHostTestHarness::TearDown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  void DestroyFamilyUserChromeActivityMetrics() {
    family_user_chrome_activity_metrics_.reset();
  }

  void InitiateFamilyUserChromeActivityMetrics() {
    family_user_chrome_activity_metrics_ =
        std::make_unique<FamilyUserChromeActivityMetrics>(profile());
  }

  void SetActiveSessionStartTime(base::Time time) {
    family_user_chrome_activity_metrics_->SetActiveSessionStartForTesting(time);
  }

  void OnNewDay() { family_user_chrome_activity_metrics_->OnNewDay(); }

  void PushChromeAppInstance(aura::Window* window, apps::InstanceState state) {
    apps::InstanceParams params(app_time::GetChromeAppId().app_id(), window);
    params.state = std::make_pair(state, base::Time::Now());
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->InstanceRegistry()
        .CreateOrUpdateInstance(std::move(params));
  }

  std::unique_ptr<Browser> CreateBrowserWithAuraWindow() {
    std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    window->SetId(0);
    window->Init(ui::LAYER_TEXTURED);
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    browser_window_ =
        std::make_unique<TestBrowserWindowAura>(std::move(window));
    params.window = browser_window_.get();

    return std::unique_ptr<Browser>(Browser::Create(params));
  }

  void SetSessionState(session_manager::SessionState state) {
    session_manager_.SetSessionState(state);
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }

  std::unique_ptr<Browser> test_browser_;

 private:
  std::unique_ptr<FamilyUserChromeActivityMetrics>
      family_user_chrome_activity_metrics_;
  std::unique_ptr<TestBrowserWindowAura> browser_window_;
  session_manager::SessionManager session_manager_;
  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_ =
      nullptr;
};

TEST_F(FamilyUserChromeActivityMetricsTest, Basic) {
  base::HistogramTester histogram_tester;

  task_environment()->FastForwardBy(kHalfHour);

  // Set the app running in the background.
  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kInactiveInstanceState);

  EXPECT_EQ(kHalfHour,
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));

  // Test multiple browsers.
  std::unique_ptr<Browser> another_browser = CreateBrowserWithAuraWindow();
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());

  PushChromeAppInstance(another_browser->window()->GetNativeWindow(),
                        apps::InstanceState::kActive);
  task_environment()->FastForwardBy(kHalfHour);
  PushChromeAppInstance(another_browser->window()->GetNativeWindow(),
                        apps::InstanceState::kDestroyed);
  EXPECT_EQ(base::Hours(1),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));

  // Test date change.
  task_environment()->FastForwardBy(base::Days(1));
  OnNewDay();

  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));
  histogram_tester.ExpectTimeBucketCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      base::Hours(1), 1);
}

TEST_F(FamilyUserChromeActivityMetricsTest, ClockBackward) {
  base::HistogramTester histogram_tester;

  base::Time mock_session_start = base::Time::Now() + kHalfHour;

  // Mock a state that start time > end time.
  SetActiveSessionStartTime(mock_session_start);

  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kInactiveInstanceState);

  histogram_tester.ExpectTotalCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      0);
  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));
}

// Tests destroying FamilyUserChromeActivityMetrics. OnAppInactive() will be
// invoked while Chrome browser state changed to kDestroyed.
TEST_F(FamilyUserChromeActivityMetricsTest,
       DestructionAndCreationOfFamilyUserChromeActivityMetrics) {
  base::HistogramTester histogram_tester;

  task_environment()->FastForwardBy(kHalfHour);

  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        apps::InstanceState::kDestroyed);
  test_browser_.reset();
  EXPECT_EQ(0U, BrowserList::GetInstance()->size());
  DestroyFamilyUserChromeActivityMetrics();

  histogram_tester.ExpectTotalCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      0);
  EXPECT_EQ(kHalfHour,
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));

  // Test restart.
  InitiateFamilyUserChromeActivityMetrics();
  test_browser_ = CreateBrowserWithAuraWindow();
  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kActiveInstanceState);
  task_environment()->FastForwardBy(kHalfHour);

  // Set the app running background.
  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kInactiveInstanceState);

  histogram_tester.ExpectTotalCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      0);
  EXPECT_EQ(base::Hours(1),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));
}

TEST_F(FamilyUserChromeActivityMetricsTest, ScreenStateChange) {
  base::HistogramTester histogram_tester;
  // Set UsageTimeStateNotifier::UsageTimeState active.
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Set the screen off for half an hour.
  SetScreenOff(true);
  task_environment()->FastForwardBy(kHalfHour);

  // Set the screen on. Set the app inactive after 1 minute.
  SetScreenOff(false);
  task_environment()->FastForwardBy(kOneMinute);
  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kInactiveInstanceState);
  EXPECT_EQ(kOneMinute,
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));

  // Test the screen off for 1 day.
  SetScreenOff(true);

  task_environment()->FastForwardBy(base::Days(1));
  OnNewDay();

  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));
  histogram_tester.ExpectTimeBucketCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      kOneMinute, 1);
}

// When lock or unlock the screen, both
// FamilyUserChromeActivityMetrics::OnAppInactive() and
// FamilyUserChromeActivityMetrics::OnUsageTimeStateChange() get called.
TEST_F(FamilyUserChromeActivityMetricsTest, MockLockAndUnclockScreen) {
  base::HistogramTester histogram_tester;
  // Set UsageTimeStateNotifier::UsageTimeState active.
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Set the app active for 1 minute.
  task_environment()->FastForwardBy(kOneMinute);

  // Mock screen locked for half an hour.
  SetSessionState(session_manager::SessionState::LOCKED);
  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kInactiveInstanceState);
  task_environment()->FastForwardBy(kHalfHour);

  // Mock unlocking screen.
  SetSessionState(session_manager::SessionState::ACTIVE);
  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kActiveInstanceState);

  // Set the app inactive after 1 minute.
  task_environment()->FastForwardBy(kOneMinute);
  PushChromeAppInstance(test_browser_->window()->GetNativeWindow(),
                        kInactiveInstanceState);

  EXPECT_EQ(base::Minutes(2),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));

  task_environment()->FastForwardBy(base::Days(1));
  OnNewDay();

  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));
  histogram_tester.ExpectTimeBucketCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      base::Minutes(2), 1);
}

}  // namespace ash
