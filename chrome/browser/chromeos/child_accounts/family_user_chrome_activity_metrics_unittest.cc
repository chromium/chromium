// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_chrome_activity_metrics.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace chromeos {

namespace {
constexpr char kExtensionNameChrome[] = "Chrome";
constexpr char kExtensionAppUrl[] = "https://example.com/";
constexpr base::TimeDelta kHalfHour = base::TimeDelta::FromMinutes(30);

constexpr apps::InstanceState kActiveInstanceState =
    static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
constexpr apps::InstanceState kInactiveInstanceState =
    static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                     apps::InstanceState::kRunning);

}  // namespace

class FamilyUserChromeActivityMetricsTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FamilyUserChromeActivityMetricsTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~FamilyUserChromeActivityMetricsTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InitiateFamilyUserChromeActivityMetrics();

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        /*command_line=*/base::CommandLine::ForCurrentProcess(),
        /*install_directory=*/base::FilePath(), /*autoupdate_enabled=*/false);
    extension_service_->Init();

    // Install Chrome.
    scoped_refptr<extensions::Extension> chrome = app_time::CreateExtension(
        extension_misc::kChromeAppId, kExtensionNameChrome, kExtensionAppUrl);
    extension_service_->AddComponentExtension(chrome.get());

    PushChromeApp();
  }

  void TearDown() override {
    DestroyFamilyUserChromeActivityMetrics();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void DestroyFamilyUserChromeActivityMetrics() {
    family_user_chrome_activity_metrics_.reset();
  }

  void InitiateFamilyUserChromeActivityMetrics() {
    family_user_chrome_activity_metrics_ =
        std::make_unique<FamilyUserChromeActivityMetrics>(profile());
  }

  void PushChromeApp() {
    std::vector<apps::mojom::AppPtr> deltas;
    auto app = apps::mojom::App::New();
    app->app_id = app_time::GetChromeAppId().app_id();
    app->app_type = app_time::GetChromeAppId().app_type();
    deltas.push_back(std::move(app));

    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppRegistryCache()
        .OnApps(std::move(deltas), app_time::GetChromeAppId().app_type(),
                false /* should_notify_initialized */);
  }

  void SetActiveSessionStartTime(base::Time time) {
    family_user_chrome_activity_metrics_->SetActiveSessionStartForTesting(time);
  }

  void OnNewDay() { family_user_chrome_activity_metrics_->OnNewDay(); }

  void PushChromeAppInstance(aura::Window* window, apps::InstanceState state) {
    std::unique_ptr<apps::Instance> instance = std::make_unique<apps::Instance>(
        app_time::GetChromeAppId().app_id(), window);
    instance->UpdateState(state, base::Time::Now());

    std::vector<std::unique_ptr<apps::Instance>> deltas;
    deltas.push_back(std::move(instance));

    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->InstanceRegistry()
        .OnInstances(deltas);
  }

  std::unique_ptr<Browser> CreateBrowserWithAuraWindow() {
    std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    window->set_id(0);
    window->Init(ui::LAYER_TEXTURED);
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    browser_window_ =
        std::make_unique<TestBrowserWindowAura>(std::move(window));
    params.window = browser_window_.get();

    return std::unique_ptr<Browser>(Browser::Create(params));
  }

  PrefService* pref_service() { return profile()->GetPrefs(); }

 private:
  std::unique_ptr<FamilyUserChromeActivityMetrics>
      family_user_chrome_activity_metrics_;
  std::unique_ptr<TestBrowserWindowAura> browser_window_;

  extensions::ExtensionService* extension_service_ = nullptr;
};

TEST_F(FamilyUserChromeActivityMetricsTest, Basic) {
  base::HistogramTester histogram_tester;

  BrowserList* active_browser_list = BrowserList::GetInstance();
  // Expect BrowserList is empty at the beginning.
  EXPECT_EQ(0U, active_browser_list->size());
  std::unique_ptr<Browser> browser1 = CreateBrowserWithAuraWindow();

  EXPECT_EQ(1U, active_browser_list->size());

  // Set the app active. If the app is active, it should be started, running,
  // and visible.

  PushChromeAppInstance(browser1->window()->GetNativeWindow(),
                        kActiveInstanceState);
  task_environment()->FastForwardBy(kHalfHour);

  // Set the app running in the background.
  PushChromeAppInstance(browser1->window()->GetNativeWindow(),
                        kInactiveInstanceState);

  EXPECT_EQ(kHalfHour,
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));

  // Test multiple browsers.
  std::unique_ptr<Browser> browser2 = CreateBrowserWithAuraWindow();
  EXPECT_EQ(2U, active_browser_list->size());

  PushChromeAppInstance(browser2->window()->GetNativeWindow(),
                        apps::InstanceState::kActive);
  task_environment()->FastForwardBy(kHalfHour);
  PushChromeAppInstance(browser2->window()->GetNativeWindow(),
                        apps::InstanceState::kDestroyed);
  EXPECT_EQ(base::TimeDelta::FromHours(1),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));

  // Test date change.
  task_environment()->FastForwardBy(base::TimeDelta::FromDays(1));
  OnNewDay();

  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));
  histogram_tester.ExpectTimeBucketCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      base::TimeDelta::FromHours(1), 1);
}

TEST_F(FamilyUserChromeActivityMetricsTest, ClockBackward) {
  base::HistogramTester histogram_tester;

  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(0U, active_browser_list->size());
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow();

  // Expect that |browser| is added to browser list.
  EXPECT_EQ(1U, active_browser_list->size());

  PushChromeAppInstance(browser->window()->GetNativeWindow(),
                        kActiveInstanceState);

  base::Time mock_session_start = base::Time::Now() + kHalfHour;

  // Mock a state that start time > end time.
  SetActiveSessionStartTime(mock_session_start);

  PushChromeAppInstance(browser->window()->GetNativeWindow(),
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

  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(0U, active_browser_list->size());
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow();

  // Expect that |browser| is added to browser list.
  EXPECT_EQ(1U, active_browser_list->size());

  PushChromeAppInstance(browser->window()->GetNativeWindow(),
                        kActiveInstanceState);

  task_environment()->FastForwardBy(kHalfHour);

  PushChromeAppInstance(browser->window()->GetNativeWindow(),
                        apps::InstanceState::kDestroyed);
  browser.reset();
  EXPECT_EQ(0U, active_browser_list->size());
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
  browser = CreateBrowserWithAuraWindow();
  PushChromeAppInstance(browser->window()->GetNativeWindow(),
                        kActiveInstanceState);
  task_environment()->FastForwardBy(kHalfHour);

  // Set the app running background.
  PushChromeAppInstance(browser->window()->GetNativeWindow(),
                        kInactiveInstanceState);

  histogram_tester.ExpectTotalCount(
      FamilyUserChromeActivityMetrics::
          kChromeBrowserEngagementDurationHistogramName,
      0);
  EXPECT_EQ(base::TimeDelta::FromHours(1),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsChromeBrowserEngagementDuration));
}

}  // namespace chromeos
