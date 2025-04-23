// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate_map.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/scoped_set_idle_state.h"
#include "url/gurl.h"

namespace ash {

namespace {
using testing::ElementsAre;

const char kSettingsAppInternalName[] = "OSSettings";
const char kCameraAppInternalName[] = "Camera";

GURL AppUrl1() {
  return GURL(content::GetWebUIURL("system-app1"));
}
GURL AppUrl2() {
  return GURL(content::GetWebUIURL("system-app2"));
}
GURL AppUrl3() {
  return GURL(content::GetWebUIURL("system-app3"));
}

std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInstallInfo(
    const GURL& url) {
  std::unique_ptr<web_app::WebAppInstallInfo> info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(url);
  info->scope = url.GetWithoutFilename();
  info->title = u"Web App";
  return info;
}

std::unique_ptr<web_app::WebAppInstallInfo> GetNullWebAppInstallInfo() {
  return nullptr;
}

web_app::WebAppInstallInfoFactory GetApp1WebAppInfoFactory() {
  // "static" so that web_app::ExternalInstallOptions comparisons in tests work.
  static auto factory = base::BindRepeating(&GetWebAppInstallInfo, AppUrl1());
  return factory;
}

web_app::WebAppInstallInfoFactory GetApp2WebAppInfoFactory() {
  // "static" so that web_app::ExternalInstallOptions comparisons in tests work.
  static auto factory = base::BindRepeating(&GetWebAppInstallInfo, AppUrl2());
  return factory;
}

web_app::WebAppInstallInfoFactory GetNullWebAppInfoFactory() {
  static auto factory = base::BindRepeating(&GetNullWebAppInstallInfo);
  return factory;
}

class SystemWebAppWaiter {
 public:
  explicit SystemWebAppWaiter(SystemWebAppManager* system_web_app_manager) {
    system_web_app_manager->ResetForTesting();
    system_web_app_manager->on_apps_synchronized().Post(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Wait one execution loop for on_apps_synchronized() to be called on
          // all listeners.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, run_loop_.QuitClosure());
        }));
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

class TestUiManagerObserver : public web_app::WebAppUiManagerObserver {
 public:
  explicit TestUiManagerObserver(web_app::WebAppUiManager* ui_manager) {
    ui_manager_observation_.Observe(ui_manager);
  }

  using ReadyToCommitNavigationCallback = base::RepeatingCallback<void(
      const webapps::AppId& app_id,
      content::NavigationHandle* navigation_handle)>;

  void SetReadyToCommitNavigationCallback(
      ReadyToCommitNavigationCallback callback) {
    ready_to_commit_navigation_callback_ = std::move(callback);
  }

  void OnReadyToCommitNavigation(
      const webapps::AppId& app_id,
      content::NavigationHandle* navigation_handle) override {
    if (ready_to_commit_navigation_callback_)
      ready_to_commit_navigation_callback_.Run(app_id, navigation_handle);
  }

  using UiManagerDestroyedCallback = base::RepeatingCallback<void()>;

  void SetUiManagerDestroyedCallback(UiManagerDestroyedCallback callback) {
    ui_manager_destroyed_callback_ = std::move(callback);
  }

  void OnWebAppUiManagerDestroyed() override {
    if (ui_manager_destroyed_callback_)
      ui_manager_destroyed_callback_.Run();
    ui_manager_observation_.Reset();
  }

 private:
  ReadyToCommitNavigationCallback ready_to_commit_navigation_callback_;
  UiManagerDestroyedCallback ui_manager_destroyed_callback_;

  base::ScopedObservation<web_app::WebAppUiManager,
                          web_app::WebAppUiManagerObserver>
      ui_manager_observation_{this};
};

}  // namespace

class SystemWebAppManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit SystemWebAppManagerTest(TaskEnvironmentTraits&&... traits)
      : ChromeRenderViewHostTestHarness(
            std::forward<TaskEnvironmentTraits>(traits)...) {}
  SystemWebAppManagerTest(const SystemWebAppManagerTest&) = delete;
  SystemWebAppManagerTest& operator=(const SystemWebAppManagerTest&) = delete;

  ~SystemWebAppManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  web_app::FakeWebAppProvider& provider() {
    return static_cast<web_app::FakeWebAppProvider&>(
        *SystemWebAppManager::GetWebAppProvider(profile()));
  }

  TestSystemWebAppManager& system_web_app_manager() {
    return static_cast<TestSystemWebAppManager&>(
        *SystemWebAppManager::Get(profile()));
  }

  web_app::ExternallyManagedAppManager& externally_managed_app_manager() {
    return provider().externally_managed_app_manager();
  }

  bool IsInstalled(const GURL& install_url) {
    return provider().registrar_unsafe().GetInstallState(
               GetAppIdFromInstallUrl(install_url)) ==
           web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
  }

  bool WasReinstalled(const GURL& install_url) {
    const webapps::AppId& app_id = GetAppIdFromInstallUrl(install_url);
    return provider().registrar_unsafe().GetAppLatestInstallTime(app_id) >
           provider().registrar_unsafe().GetAppFirstInstallTime(app_id);
  }

  const webapps::AppId GetAppIdFromInstallUrl(const GURL install_url) {
    return web_app::GenerateAppId(/*manifest_id=*/std::nullopt, install_url);
  }

  bool IsVersionCorrect(const base::Version& current_version) {
    return system_web_app_manager().CurrentVersion() == current_version;
  }

  void StartAndWaitForAppsToSynchronize() {
    SystemWebAppWaiter waiter(&system_web_app_manager());
    system_web_app_manager().Start();
    waiter.Wait();
  }

  void StartAndWaitForIconCheck() {
    StartAndWaitForAppsToSynchronize();

    base::RunLoop run_loop;
    system_web_app_manager().on_icon_check_completed().Post(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void AwaitSystemWebAppCommandsCompletePostStartup() {
    base::RunLoop().RunUntilIdle();
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
};

// Test that changing the set of System Apps uninstalls apps.
TEST_F(SystemWebAppManagerTest, UninstallAppInstalledInPreviousSession) {
  // Simulate System Apps and a regular app that were installed in the
  // previous session.
  web_app::test::InstallDummyWebApp(
      profile(), "App1", AppUrl1(),
      webapps::WebappInstallSource::SYSTEM_DEFAULT);
  web_app::test::InstallDummyWebApp(
      profile(), "App2", AppUrl2(),
      webapps::WebappInstallSource::SYSTEM_DEFAULT);
  web_app::test::InstallDummyWebApp(
      profile(), "App3", AppUrl3(),
      webapps::WebappInstallSource::INTERNAL_DEFAULT);

  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(SystemWebAppType::SETTINGS,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::SETTINGS, kSettingsAppInternalName,
                          AppUrl1(), GetApp1WebAppInfoFactory()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  StartAndWaitForAppsToSynchronize();

  // `AppUrl1()` should be installed because it is in the
  // `SystemWebAppDelegateMap` list. `AppUrl3()` should be installed because it
  // is a default app.
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(IsInstalled(AppUrl2()));
  EXPECT_TRUE(IsInstalled(AppUrl3()));
}

TEST_F(SystemWebAppManagerTest, AlwaysUpdate) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate);

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }
  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.0")));

  // Create another app. The version hasn't changed but the app should still
  // install.
  {
    SystemWebAppDelegateMap system_apps;

    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_apps.emplace(SystemWebAppType::CAMERA,
                        std::make_unique<UnittestingSystemAppDelegate>(
                            SystemWebAppType::CAMERA, kCameraAppInternalName,
                            AppUrl2(), GetApp2WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }
  // This one returns because on_apps_synchronized runs immediately.
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));
}

TEST_F(SystemWebAppManagerTest, UpdateOnVersionChange) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
    system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  }
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.0")));

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    // Create another app. The version hasn't changed, but we should immediately
    // install anyway, as if a user flipped a chrome://flag. The first app won't
    // force reinstall.
    system_apps.emplace(SystemWebAppType::CAMERA,
                        std::make_unique<UnittestingSystemAppDelegate>(
                            SystemWebAppType::CAMERA, kCameraAppInternalName,
                            AppUrl2(), GetApp2WebAppInfoFactory()));

    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("2.0.0.0")));

  // Changing the install URL of a system app propagates even without a
  // version change.
  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl3(),
            base::BindRepeating(&GetWebAppInstallInfo, AppUrl3())));

    system_apps.emplace(SystemWebAppType::CAMERA,
                        std::make_unique<UnittestingSystemAppDelegate>(
                            SystemWebAppType::CAMERA, kCameraAppInternalName,
                            AppUrl2(), GetApp2WebAppInfoFactory()));

    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }

  StartAndWaitForAppsToSynchronize();
  EXPECT_FALSE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));
  EXPECT_TRUE(IsInstalled(AppUrl3()));
}

TEST_F(SystemWebAppManagerTest, UpdateOnVersionChangeEvenIfIconsBroken) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }
  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.0")));

  // Simulate something going wrong in the interim.
  system_web_app_manager().set_icons_are_broken(true);
  // Must be greater than `kInstallFailureAttempts` constant.
  profile()->GetPrefs()->SetInteger(
      prefs::kSystemWebAppInstallFailureCount,
      SystemWebAppManager::kInstallFailureAttempts + 1);

  system_web_app_manager().set_current_version(base::Version("1.0.0.1"));
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.1")));
}

TEST_F(SystemWebAppManagerTest, RetryBrokenIcons) {
  // We don't want to force reinstall by default, we want to check that we
  // correctly set to force reinstall when icons are broken.
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }

  {
    // Initial install.
    StartAndWaitForAppsToSynchronize();
    EXPECT_TRUE(IsInstalled(AppUrl1()));
    EXPECT_FALSE(WasReinstalled(AppUrl1()));
  }

  {
    // Icons not broken.
    system_web_app_manager().set_icons_are_broken(false);
    StartAndWaitForAppsToSynchronize();
    EXPECT_FALSE(WasReinstalled(AppUrl1()));
  }

  {
    // Broken icons should force reinstall.
    system_web_app_manager().set_icons_are_broken(true);
    StartAndWaitForAppsToSynchronize();
    EXPECT_TRUE(WasReinstalled(AppUrl1()));
  }
}

TEST_F(SystemWebAppManagerTest, AbortOnExceedRetryLimit) {
  base::HistogramTester histograms;

  // We don't want to force reinstall by default, we want to check that we
  // correctly set to force reinstall when icons are broken.
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
    system_web_app_manager().set_icons_are_broken(true);
  }

  {
    // Initial install
    StartAndWaitForAppsToSynchronize();
    EXPECT_TRUE(IsInstalled(AppUrl1()));
    EXPECT_FALSE(WasReinstalled(AppUrl1()));
  }

  {
    // 1st retry
    StartAndWaitForIconCheck();
    histograms.ExpectBucketCount(
        SystemWebAppManager::kIconsFixedOnReinstallHistogramName, false, 1);
  }

  {
    // 2nd retry
    StartAndWaitForIconCheck();
    histograms.ExpectBucketCount(
        SystemWebAppManager::kIconsFixedOnReinstallHistogramName, false, 2);
  }

  {
    // 3rd retry
    StartAndWaitForIconCheck();
    histograms.ExpectBucketCount(
        SystemWebAppManager::kIconsFixedOnReinstallHistogramName, false, 3);
  }

  {
    // 4th retry should be aborted - no new measurement of
    // `kIconsFixedOnReinstallHistogramName`
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    base::RunLoop().RunUntilIdle();
    histograms.ExpectBucketCount(
        SystemWebAppManager::kIconsFixedOnReinstallHistogramName, false, 3);
  }
}

TEST_F(SystemWebAppManagerTest, UpdateOnLocaleChange) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(SystemWebAppType::SETTINGS,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::SETTINGS, kSettingsAppInternalName,
                          AppUrl1(), GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  // First execution.
  system_web_app_manager().set_current_locale("en-US");
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(WasReinstalled(AppUrl1()));

  // Change locale setting, should trigger reinstall.
  system_web_app_manager().set_current_locale("ja");
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(WasReinstalled(AppUrl1()));

  base::Time last_retry_time =
      provider().registrar_unsafe().GetAppLatestInstallTime(
          GetAppIdFromInstallUrl(AppUrl1()));

  // Do not reinstall because locale is not changed.
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  base::Time retry_time_post_synchronize =
      provider().registrar_unsafe().GetAppLatestInstallTime(
          GetAppIdFromInstallUrl(AppUrl1()));
  EXPECT_EQ(last_retry_time, retry_time_post_synchronize);
}

TEST_F(SystemWebAppManagerTest, InstallResultHistogram) {
  base::HistogramTester histograms;
  const std::string settings_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kSettingsAppInternalName;
  const std::string camera_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kCameraAppInternalName;
  // Profile category for Chrome OS testing environment is "Other".
  const std::string profile_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) +
      ".Profiles.Other";

  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate);

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 0);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 0);
    histograms.ExpectTotalCount(profile_install_result_histogram, 0);
    histograms.ExpectTotalCount(
        SystemWebAppManager::kFreshInstallDurationHistogramName, 0);

    StartAndWaitForAppsToSynchronize();

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 1);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 1);
    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
    histograms.ExpectTotalCount(profile_install_result_histogram, 1);
    histograms.ExpectBucketCount(
        profile_install_result_histogram,
        webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
    histograms.ExpectTotalCount(
        SystemWebAppManager::kFreshInstallDurationHistogramName, 1);
  }

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetNullWebAppInfoFactory()));
    system_apps.emplace(SystemWebAppType::CAMERA,
                        std::make_unique<UnittestingSystemAppDelegate>(
                            SystemWebAppType::CAMERA, kCameraAppInternalName,
                            AppUrl2(), GetNullWebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

    StartAndWaitForAppsToSynchronize();

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 3);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        webapps::InstallResultCode::kGetWebAppInstallInfoFailed, 2);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 2);
    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        webapps::InstallResultCode::kGetWebAppInstallInfoFailed, 1);

    histograms.ExpectBucketCount(
        camera_app_install_result_histogram,
        webapps::InstallResultCode::kGetWebAppInstallInfoFailed, 1);
  }

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

    histograms.ExpectTotalCount(
        SystemWebAppManager::kFreshInstallDurationHistogramName, 2);
    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 0);
    histograms.ExpectBucketCount(
        profile_install_result_histogram,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 0);

    {
      SystemWebAppWaiter waiter(&system_web_app_manager());
      system_web_app_manager().Start();
      system_web_app_manager().Shutdown();
      waiter.Wait();
    }

    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        webapps::InstallResultCode::kGetWebAppInstallInfoFailed, 2);

    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    histograms.ExpectBucketCount(
        profile_install_result_histogram,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);

    // If install was interrupted by shutdown, do not report duration.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kFreshInstallDurationHistogramName, 2);
  }
}

TEST_F(SystemWebAppManagerTest,
       InstallDurationHistogram_ExcludeNonForceInstall) {
  base::HistogramTester histograms;

  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(SystemWebAppType::SETTINGS,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::SETTINGS, kSettingsAppInternalName,
                          AppUrl1(), GetApp1WebAppInfoFactory()));
  system_apps.emplace(SystemWebAppType::CAMERA,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::CAMERA, kCameraAppInternalName,
                          AppUrl2(), GetApp2WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  {
    StartAndWaitForAppsToSynchronize();

    // The install duration histogram should be recorded, because the first
    // install happens on a clean profile.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kFreshInstallDurationHistogramName, 1);
  }

  {
    StartAndWaitForAppsToSynchronize();

    // Don't record install duration histogram, because this time we don't ask
    // to force install all apps.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kFreshInstallDurationHistogramName, 1);
  }
}

TEST_F(SystemWebAppManagerTest, AbandonFailedInstalls) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(SystemWebAppType::SETTINGS,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::SETTINGS, kSettingsAppInternalName,
                          AppUrl1(), GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(WasReinstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.0")));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  //
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));

  // We use RunUntilIdle because the install requests are dropped, so
  // on_app_synchronized() won't be called.
  {
    web_app::ExternallyManagedAppManager::ScopedDropRequestsForTesting
        drop_requests_for_testing;
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();

    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();

    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();

    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();
    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  }

  // All retries were abandoned, so the app will not be reinstalled.
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(WasReinstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("2.0.0.0")));

  // Start running requests again. If we don't abandon at the same version, it
  // doesn't even attempt another request.
  system_web_app_manager().ResetForTesting();
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  system_web_app_manager().Start();
  AwaitSystemWebAppCommandsCompletePostStartup();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_FALSE(WasReinstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("2.0.0.0")));

  // Bump the version, and it works.
  system_web_app_manager().ResetForTesting();
  system_web_app_manager().set_current_version(base::Version("3.0.0.0"));
  system_web_app_manager().Start();
  AwaitSystemWebAppCommandsCompletePostStartup();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_TRUE(WasReinstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("3.0.0.0")));
}

// Same test, but for locale change.
TEST_F(SystemWebAppManagerTest, AbandonFailedInstallsLocaleChange) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(SystemWebAppType::SETTINGS,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::SETTINGS, kSettingsAppInternalName,
                          AppUrl1(), GetApp1WebAppInfoFactory()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  system_web_app_manager().set_current_locale("en/us");
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(WasReinstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.0")));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  system_web_app_manager().set_current_locale("en/au");
  system_web_app_manager().ResetForTesting();

  {
    web_app::ExternallyManagedAppManager::ScopedDropRequestsForTesting
        drop_requests_for_testing;
    // We use RunUntilIdle because the install requests are dropped, so
    // on_app_synchronized() won't be called.
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();

    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();

    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();

    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();
    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  }

  // All retries were abandoned, so the app will not be reinstalled.
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(WasReinstalled(AppUrl1()));

  // Start running requests again. If we don't abandon at the same version, it
  // doesn't even attempt another request.
  system_web_app_manager().ResetForTesting();
  system_web_app_manager().Start();
  AwaitSystemWebAppCommandsCompletePostStartup();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();

  // Verify that a reinstall did not happen.
  EXPECT_FALSE(WasReinstalled(AppUrl1()));

  // Bump the version, and it works.
  system_web_app_manager().ResetForTesting();
  system_web_app_manager().set_current_locale("fr/fr");
  system_web_app_manager().Start();
  AwaitSystemWebAppCommandsCompletePostStartup();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();

  // Verify reinstall happened by comparing the latest install times.
  EXPECT_TRUE(WasReinstalled(AppUrl1()));
}

TEST_F(SystemWebAppManagerTest, SucceedsAfterOneRetry) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  // Set up and install a baseline
  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(SystemWebAppType::SETTINGS,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::SETTINGS, kSettingsAppInternalName,
                          AppUrl1(), GetApp1WebAppInfoFactory()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(WasReinstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.0")));

  // Bump the version number, and an update will trigger, and force
  // reinstallation. But, this fails!
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  {
    web_app::ExternallyManagedAppManager::ScopedDropRequestsForTesting
        drop_requests_for_testing;
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();
    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();

    EXPECT_TRUE(IsInstalled(AppUrl1()));
    EXPECT_FALSE(WasReinstalled(AppUrl1()));

    // Retry a few times, but not until abandonment.
    system_web_app_manager().ResetForTesting();
    system_web_app_manager().Start();
    AwaitSystemWebAppCommandsCompletePostStartup();
    externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  }

  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_FALSE(WasReinstalled(AppUrl1()));

  // Now we succeed at the same version
  StartAndWaitForAppsToSynchronize();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();

  StartAndWaitForAppsToSynchronize();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_TRUE(WasReinstalled(AppUrl1()));
  EXPECT_TRUE(IsVersionCorrect(base::Version("2.0.0.0")));
  base::Time last_reinstall_time =
      provider().registrar_unsafe().GetAppLatestInstallTime(
          GetAppIdFromInstallUrl(AppUrl1()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps. This succeeds, everything works.
  system_web_app_manager().set_current_version(base::Version("3.0.0.0"));

  StartAndWaitForAppsToSynchronize();
  externally_managed_app_manager().ClearSynchronizeRequestsForTesting();
  StartAndWaitForAppsToSynchronize();
  base::Time install_time_post_synchronize =
      provider().registrar_unsafe().GetAppLatestInstallTime(
          GetAppIdFromInstallUrl(AppUrl1()));
  EXPECT_GT(install_time_post_synchronize, last_reinstall_time);
  EXPECT_TRUE(IsVersionCorrect(base::Version("3.0.0.0")));
}

TEST_F(SystemWebAppManagerTest, ForceReinstallFeature) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  // Register a test system app.
  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(SystemWebAppType::SETTINGS,
                      std::make_unique<UnittestingSystemAppDelegate>(
                          SystemWebAppType::SETTINGS, kSettingsAppInternalName,
                          AppUrl1(), GetApp1WebAppInfoFactory()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  // Install the App normally.
  {
    StartAndWaitForAppsToSynchronize();
    EXPECT_TRUE(IsInstalled(AppUrl1()));
  }

  // Enable AlwaysReinstallSystemWebApps feature, verify force_reinstall is set.
  {
    base::test::ScopedFeatureList feature_reinstall;
    feature_reinstall.InitAndEnableFeature(
        features::kAlwaysReinstallSystemWebApps);

    StartAndWaitForAppsToSynchronize();
    EXPECT_TRUE(IsInstalled(AppUrl1()));
    EXPECT_TRUE(WasReinstalled(AppUrl1()));
  }
}

TEST_F(SystemWebAppManagerTest, IsSWABeforeSync) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  // Set up and install a baseline
  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }
  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(system_web_app_manager().IsSystemWebApp(
      web_app::GenerateAppId(/*manifest_id=*/std::nullopt, AppUrl1())));
  EXPECT_TRUE(IsVersionCorrect(base::Version("1.0.0.0")));

  auto unsynced_system_web_app_manager =
      std::make_unique<TestSystemWebAppManager>(profile());

  {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<UnittestingSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory()));
    unsynced_system_web_app_manager->SetSystemAppsForTesting(
        std::move(system_apps));
  }

  EXPECT_TRUE(unsynced_system_web_app_manager->IsSystemWebApp(
      web_app::GenerateAppId(/*manifest_id=*/std::nullopt, AppUrl1())));
}

class TimerSystemAppDelegate : public UnittestingSystemAppDelegate {
 public:
  TimerSystemAppDelegate(SystemWebAppType type,
                         const std::string& name,
                         const GURL& url,
                         web_app::WebAppInstallInfoFactory info_factory,
                         std::optional<base::TimeDelta> period,
                         bool open_immediately)
      : UnittestingSystemAppDelegate(type, name, url, std::move(info_factory)),
        period_(period),
        open_immediately_(open_immediately) {}
  std::optional<SystemWebAppBackgroundTaskInfo> GetTimerInfo() const override;

 private:
  std::optional<base::TimeDelta> period_;
  bool open_immediately_;
};

std::optional<SystemWebAppBackgroundTaskInfo>
TimerSystemAppDelegate::GetTimerInfo() const {
  return SystemWebAppBackgroundTaskInfo(period_, GetInstallUrl(),
                                        open_immediately_);
}

class SystemWebAppManagerTimerTest : public SystemWebAppManagerTest {
 public:
  SystemWebAppManagerTimerTest()
      : SystemWebAppManagerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetupTimer(std::optional<base::TimeDelta> period,
                  bool open_immediately) {
    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(
        SystemWebAppType::SETTINGS,
        std::make_unique<TimerSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory(), period, open_immediately));

    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  }

  void TearDown() override {
    // Normally, WebContents used to perform background tasks are released
    // during KeyedService shutdown. In tests, we need to release them before
    // fixture tear down.
    //
    // The parent fixture (RenderViewHostTestHarness::TearDown) expects
    // us to release WebContents before tearing down (which happens before
    // KeyedService shutdown because the parent fixture owns TestingProfile).
    //
    // If we don't StopBackgroundTasks (and release WebContents) here, the
    // fixture will complain about leaking RenderWidgetHost.
    system_web_app_manager().StopBackgroundTasksForTesting();
    SystemWebAppManagerTest::TearDown();
  }
};

TEST_F(SystemWebAppManagerTimerTest, BackgroundTaskDisabled) {
  // 1) Disabled app should not push to background tasks.
  {
    std::unique_ptr<TimerSystemAppDelegate> sys_app_delegate =
        std::make_unique<TimerSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory(), base::Seconds(60), false);

    sys_app_delegate->SetIsAppEnabled(false);

    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(SystemWebAppType::SETTINGS,
                        std::move(sys_app_delegate));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
    StartAndWaitForAppsToSynchronize();

    EXPECT_EQ(0u,
              system_web_app_manager().GetBackgroundTasksForTesting().size());
  }

  // 2) Enabled app should push to background tasks.
  {
    std::unique_ptr<TimerSystemAppDelegate> sys_app_delegate =
        std::make_unique<TimerSystemAppDelegate>(
            SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
            GetApp1WebAppInfoFactory(), base::Seconds(60), false);

    SystemWebAppDelegateMap system_apps;
    system_apps.emplace(SystemWebAppType::SETTINGS,
                        std::move(sys_app_delegate));
    system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
    StartAndWaitForAppsToSynchronize();

    EXPECT_EQ(1u,
              system_web_app_manager().GetBackgroundTasksForTesting().size());
  }
}

TEST_F(SystemWebAppManagerTimerTest, TestTimer) {
  ui::ScopedSetIdleState idle(ui::IDLE_STATE_IDLE);
  SetupTimer(base::Seconds(60), false);
  StartAndWaitForAppsToSynchronize();

  auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();
  EXPECT_EQ(1u, timers.size());
  EXPECT_EQ(false, timers[0]->open_immediately_for_testing());

  auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
  web_app::TestWebAppUrlLoader* loader = url_loader.get();
  timers[0]->SetUrlLoaderForTesting(std::move(url_loader));
  loader->SetNextLoadUrlResult(AppUrl1(),
                               webapps::WebAppUrlLoaderResult::kUrlLoaded);

  EXPECT_EQ(base::Seconds(60), timers[0]->period_for_testing());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(SystemWebAppBackgroundTask::INITIAL_WAIT,
            timers[0]->get_state_for_testing());
  EXPECT_EQ(0u, timers[0]->opened_count_for_testing());
  // Fast forward until the timer fires.
  task_environment()->FastForwardBy(base::Seconds(
      SystemWebAppBackgroundTask::kInitialWaitForBackgroundTasksSeconds));
  EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(1u, timers[0]->opened_count_for_testing());

  loader->SetNextLoadUrlResult(AppUrl1(),
                               webapps::WebAppUrlLoaderResult::kUrlLoaded);

  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_PERIOD,
            timers[0]->get_state_for_testing());

  task_environment()->FastForwardBy(base::Seconds(60));

  EXPECT_EQ(2u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(2u, timers[0]->opened_count_for_testing());

  loader->SetNextLoadUrlResult(
      AppUrl1(), webapps::WebAppUrlLoaderResult::kFailedUnknownReason);

  task_environment()->FastForwardBy(base::Seconds(61));

  EXPECT_EQ(3u, timers[0]->timer_activated_count_for_testing());
  // The timer fired, but we couldn't open the page.
  EXPECT_EQ(2u, timers[0]->opened_count_for_testing());
}

TEST_F(SystemWebAppManagerTimerTest,
       TestTimerStartsImmediatelyThenRunsPeriodic) {
  ui::ScopedSetIdleState idle(ui::IDLE_STATE_IDLE);
  SetupTimer(base::Seconds(300), true);
  web_app::TestWebAppUrlLoader* loader = nullptr;
  SystemWebAppWaiter waiter(&system_web_app_manager());

  // We need to wait until the web contents and url loader are created to
  // intercept the url loader with a web_app::TestWebAppUrlLoader. Do that by
  // having a hook into on_apps_synchronized.
  system_web_app_manager().on_apps_synchronized().Post(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();

        auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
        loader = url_loader.get();
        timers[0]->SetUrlLoaderForTesting(std::move(url_loader));
        loader->SetNextLoadUrlResult(
            AppUrl1(), webapps::WebAppUrlLoaderResult::kUrlLoaded);
      }));
  system_web_app_manager().Start();
  waiter.Wait();

  auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();
  EXPECT_EQ(SystemWebAppBackgroundTask::INITIAL_WAIT,
            timers[0]->get_state_for_testing());
  task_environment()->FastForwardBy(base::Seconds(121));
  EXPECT_EQ(1u, timers.size());
  EXPECT_EQ(true, timers[0]->open_immediately_for_testing());
  EXPECT_EQ(base::Seconds(300), timers[0]->period_for_testing());
  EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(1u, timers[0]->opened_count_for_testing());
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_PERIOD,
            timers[0]->get_state_for_testing());

  loader->SetNextLoadUrlResult(AppUrl1(),
                               webapps::WebAppUrlLoaderResult::kUrlLoaded);

  task_environment()->FastForwardBy(base::Seconds(300));

  EXPECT_EQ(2u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(2u, timers[0]->opened_count_for_testing());
}

TEST_F(SystemWebAppManagerTimerTest, TestTimerStartsImmediately) {
  ui::ScopedSetIdleState idle(ui::IDLE_STATE_IDLE);
  SetupTimer(std::nullopt, true);
  web_app::TestWebAppUrlLoader* loader = nullptr;
  SystemWebAppWaiter waiter(&system_web_app_manager());

  // We need to wait until the web contents and url loader are created to
  // intercept the url loader with a web_app::TestWebAppUrlLoader. Do that by
  // having a hook into on_apps_synchronized.
  system_web_app_manager().on_apps_synchronized().Post(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();

        auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
        loader = url_loader.get();
        timers[0]->SetUrlLoaderForTesting(std::move(url_loader));
        loader->SetNextLoadUrlResult(
            AppUrl1(), webapps::WebAppUrlLoaderResult::kUrlLoaded);
      }));
  system_web_app_manager().Start();
  waiter.Wait();

  auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();
  EXPECT_EQ(SystemWebAppBackgroundTask::INITIAL_WAIT,
            timers[0]->get_state_for_testing());
  task_environment()->FastForwardBy(base::Seconds(121));
  EXPECT_EQ(1u, timers.size());
  EXPECT_EQ(true, timers[0]->open_immediately_for_testing());
  EXPECT_EQ(std::nullopt, timers[0]->period_for_testing());
  EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(1u, timers[0]->opened_count_for_testing());

  timers[0]->web_contents_for_testing()->Close();

  EXPECT_EQ(nullptr, timers[0]->web_contents_for_testing());
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_PERIOD,
            timers[0]->get_state_for_testing());

  loader->SetNextLoadUrlResult(AppUrl1(),
                               webapps::WebAppUrlLoaderResult::kUrlLoaded);

  task_environment()->FastForwardBy(base::Seconds(300));

  EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(1u, timers[0]->opened_count_for_testing());
}

TEST_F(SystemWebAppManagerTimerTest, TestTimerWaitsForIdle) {
  ui::ScopedSetIdleState scoped_active(ui::IDLE_STATE_ACTIVE);
  SetupTimer(base::Seconds(300), true);

  web_app::TestWebAppUrlLoader* loader = nullptr;
  SystemWebAppWaiter waiter(&system_web_app_manager());

  // We need to wait until the web contents and url loader are created to
  // intercept the url loader with a web_app::TestWebAppUrlLoader. Do that by
  // having a hook into on_apps_synchronized.
  system_web_app_manager().on_apps_synchronized().Post(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();

        auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
        loader = url_loader.get();
        timers[0]->SetUrlLoaderForTesting(std::move(url_loader));
        loader->SetNextLoadUrlResult(
            AppUrl1(), webapps::WebAppUrlLoaderResult::kUrlLoaded);
      }));
  system_web_app_manager().Start();
  waiter.Wait();

  auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();
  EXPECT_EQ(SystemWebAppBackgroundTask::INITIAL_WAIT,
            timers[0]->get_state_for_testing());
  task_environment()->FastForwardBy(base::Seconds(
      SystemWebAppBackgroundTask::kInitialWaitForBackgroundTasksSeconds));
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_IDLE,
            timers[0]->get_state_for_testing());
  EXPECT_EQ(1u, timers.size());
  EXPECT_EQ(true, timers[0]->open_immediately_for_testing());
  EXPECT_EQ(base::Seconds(300), timers[0]->period_for_testing());
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_IDLE,
            timers[0]->get_state_for_testing());
  EXPECT_EQ(0u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(0u, timers[0]->opened_count_for_testing());
  EXPECT_EQ(base::Time::Now(), timers[0]->polling_since_time_for_testing());

  {
    ui::ScopedSetIdleState scoped_idle(ui::IDLE_STATE_IDLE);
    task_environment()->FastForwardBy(base::Seconds(30));
    EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_PERIOD,
              timers[0]->get_state_for_testing());
    EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
    EXPECT_EQ(1u, timers[0]->opened_count_for_testing());
    EXPECT_EQ(base::Time(), timers[0]->polling_since_time_for_testing());
    loader->SetNextLoadUrlResult(AppUrl1(),
                                 webapps::WebAppUrlLoaderResult::kUrlLoaded);
    task_environment()->FastForwardBy(base::Seconds(300));

    EXPECT_EQ(2u, timers[0]->timer_activated_count_for_testing());
    EXPECT_EQ(2u, timers[0]->opened_count_for_testing());
  }
  {
    ui::ScopedSetIdleState scoped_locked(ui::IDLE_STATE_LOCKED);
    loader->SetNextLoadUrlResult(AppUrl1(),
                                 webapps::WebAppUrlLoaderResult::kUrlLoaded);
    task_environment()->FastForwardBy(base::Seconds(300));
    EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_PERIOD,
              timers[0]->get_state_for_testing());
    EXPECT_EQ(3u, timers[0]->timer_activated_count_for_testing());
    EXPECT_EQ(3u, timers[0]->opened_count_for_testing());
  }
}

TEST_F(SystemWebAppManagerTimerTest, TestTimerRunsAfterIdleLimitReached) {
  ui::ScopedSetIdleState idle(ui::IDLE_STATE_ACTIVE);
  SetupTimer(base::Seconds(300), true);

  web_app::TestWebAppUrlLoader* loader = nullptr;
  SystemWebAppWaiter waiter(&system_web_app_manager());

  // We need to wait until the web contents and url loader are created to
  // intercept the url loader with a web_app::TestWebAppUrlLoader. Do that by
  // having a hook into on_apps_synchronized.
  system_web_app_manager().on_apps_synchronized().Post(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();

        auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
        loader = url_loader.get();
        timers[0]->SetUrlLoaderForTesting(std::move(url_loader));
        loader->SetNextLoadUrlResult(
            AppUrl1(), webapps::WebAppUrlLoaderResult::kUrlLoaded);
      }));
  system_web_app_manager().Start();
  waiter.Wait();

  auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();
  EXPECT_EQ(SystemWebAppBackgroundTask::INITIAL_WAIT,
            timers[0]->get_state_for_testing());
  task_environment()->FastForwardBy(base::Seconds(
      SystemWebAppBackgroundTask::kInitialWaitForBackgroundTasksSeconds));
  EXPECT_EQ(1u, timers.size());
  EXPECT_EQ(true, timers[0]->open_immediately_for_testing());
  EXPECT_EQ(base::Seconds(300), timers[0]->period_for_testing());
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_IDLE,
            timers[0]->get_state_for_testing());
  EXPECT_EQ(0u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(0u, timers[0]->opened_count_for_testing());

  base::Time polling_since(timers[0]->polling_since_time_for_testing());
  // Poll up to not quite the maximum.
  task_environment()->FastForwardBy(base::Seconds(
      SystemWebAppBackgroundTask::kIdlePollMaxTimeToWaitSeconds - 1));
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_IDLE,
            timers[0]->get_state_for_testing());
  EXPECT_EQ(polling_since, timers[0]->polling_since_time_for_testing());
  EXPECT_EQ(0u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(0u, timers[0]->opened_count_for_testing());

  // Poll to the maximum wait.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(SystemWebAppBackgroundTask::WAIT_PERIOD,
            timers[0]->get_state_for_testing());
  EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(base::Time(), timers[0]->polling_since_time_for_testing());
  EXPECT_EQ(1u, timers[0]->opened_count_for_testing());

  loader->SetNextLoadUrlResult(AppUrl1(),
                               webapps::WebAppUrlLoaderResult::kUrlLoaded);
}

TEST_F(SystemWebAppManagerTest,
       HonorsRegisteredAppsDespiteOfPersistedWebAppInfo) {
  SystemWebAppDelegateMap system_apps;
  system_apps.emplace(
      SystemWebAppType::SETTINGS,
      std::make_unique<UnittestingSystemAppDelegate>(
          SystemWebAppType::SETTINGS, kSettingsAppInternalName, AppUrl1(),
          base::BindRepeating(&GetWebAppInstallInfo, AppUrl1())));
  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  base::RunLoop run_loop;
  system_web_app_manager().on_apps_synchronized().Post(FROM_HERE,
                                                       run_loop.QuitClosure());
  system_web_app_manager().Start();
  run_loop.Run();

  // App should be installed.
  auto opt_app_id =
      system_web_app_manager().GetAppIdForSystemApp(SystemWebAppType::SETTINGS);
  ASSERT_TRUE(opt_app_id.has_value());
  auto opt_type =
      system_web_app_manager().GetSystemAppTypeForAppId(*opt_app_id);
  ASSERT_TRUE(opt_type.has_value());
  ASSERT_EQ(SystemWebAppType::SETTINGS, *opt_type);

  // Creates a new SystemWebAppManager without the previously installed App.
  auto unsynced_system_web_app_manager =
      std::make_unique<TestSystemWebAppManager>(profile());

  // Before Apps are synchronized, WebAppRegistry should know about the App.
  const web_app::WebApp* web_app =
      provider().registrar_unsafe().GetAppById(*opt_app_id);
  ASSERT_TRUE(web_app);
  ASSERT_TRUE(web_app->client_data().system_web_app_data.has_value());
  ASSERT_EQ(SystemWebAppType::SETTINGS,
            web_app->client_data().system_web_app_data->system_app_type);

  // Checks the new SystemWebAppManager reports the App being non-SWA.
  auto opt_app_id2 = unsynced_system_web_app_manager->GetAppIdForSystemApp(
      SystemWebAppType::SETTINGS);
  EXPECT_FALSE(opt_app_id2.has_value());
  auto opt_type2 =
      unsynced_system_web_app_manager->GetSystemAppTypeForAppId(*opt_app_id);
  EXPECT_FALSE(opt_type2.has_value());
  EXPECT_FALSE(unsynced_system_web_app_manager->IsSystemWebApp(*opt_app_id));
}

TEST_F(SystemWebAppManagerTest, DestroyUiManager) {
  StartAndWaitForAppsToSynchronize();

  base::RunLoop run_loop;
  TestUiManagerObserver observer{&provider().ui_manager()};
  observer.SetUiManagerDestroyedCallback(run_loop.QuitClosure());

  // Should not crash.
  provider().ShutDownUiManagerForTesting();
  run_loop.Run();
}

class SystemWebAppManagerInKioskTest : public ChromeRenderViewHostTestHarness,
                                       public Profile::Delegate {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit SystemWebAppManagerInKioskTest(TaskEnvironmentTraits&&... traits)
      : ChromeRenderViewHostTestHarness(
            std::forward<TaskEnvironmentTraits>(traits)...) {}
  SystemWebAppManagerInKioskTest(const SystemWebAppManagerInKioskTest&) =
      delete;
  SystemWebAppManagerInKioskTest& operator=(
      const SystemWebAppManagerInKioskTest&) = delete;

  ~SystemWebAppManagerInKioskTest() override = default;

  void SetUp() override {
    // Kiosk user session needs to be set up before profile creation done in
    // ChromeRenderViewHostTestHarness::SetUp().
    user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(local_state_.Get()));
    ash::ProfileHelper::Get();  // Instantiate BrowserContextHelper.
    chromeos::SetUpFakeKioskSession();
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    user_manager_.Reset();
  }

  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    auto* user = user_manager_->GetActiveUser();
    TestingProfile::Builder builder;
    builder.SetProfileName(user->GetAccountId().GetUserEmail());
    builder.AddTestingFactories(GetTestingFactories());
    builder.SetDelegate(this);
    return builder.Build();
  }

  // Profile::Delegate:
  void OnProfileCreationStarted(Profile* profile,
                                Profile::CreateMode create_mode) override {
    ash::AnnotatedAccountId::Set(
        profile, user_manager_->GetActiveUser()->GetAccountId());
  }

  void OnProfileCreationFinished(Profile* profile,
                                 Profile::CreateMode create_mode,
                                 bool success,
                                 bool is_new_profile) override {
    // Do nothing.
  }

 private:
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  user_manager::ScopedUserManager user_manager_;
};

// Checks that SWA manager is not created in Kiosk sessions.
TEST_F(SystemWebAppManagerInKioskTest, ShouldNotCreateManagerByDefault) {
  EXPECT_FALSE(SystemWebAppManager::Get(profile()));
}

}  // namespace ash
