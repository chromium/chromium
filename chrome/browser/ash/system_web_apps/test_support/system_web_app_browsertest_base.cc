// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"

#include "base/ranges/algorithm.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

SystemWebAppBrowserTestBase::SystemWebAppBrowserTestBase() = default;
SystemWebAppBrowserTestBase::~SystemWebAppBrowserTestBase() = default;

SystemWebAppManager& SystemWebAppBrowserTestBase::GetManager() {
  auto* swa_manager = SystemWebAppManager::Get(browser()->profile());
  DCHECK(swa_manager);
  return *swa_manager;
}

SystemWebAppType SystemWebAppBrowserTestBase::GetAppType() {
  CHECK(installation_);
  return installation_->GetType();
}

void SystemWebAppBrowserTestBase::WaitForTestSystemAppInstall() {
  // Wait for the System Web Apps to install.
  if (installation_) {
    installation_->WaitForAppInstall();
  } else {
    GetManager().InstallSystemAppsForTesting();
  }
}

apps::AppLaunchParams SystemWebAppBrowserTestBase::LaunchParamsForApp(
    SystemWebAppType system_app_type) {
  std::optional<webapps::AppId> app_id =
      GetManager().GetAppIdForSystemApp(system_app_type);

  CHECK(app_id.has_value());
  return apps::AppLaunchParams(
      *app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB, apps::LaunchSource::kFromAppListGrid);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchApp(
    apps::AppLaunchParams&& params,
    bool wait_for_load,
    Browser** out_browser) {
  content::TestNavigationObserver navigation_observer(GetStartUrl(params));
  navigation_observer.StartWatchingNewWebContents();

  // AppServiceProxyFactory will DCHECK when called with wrong profile. In
  // normal scenarios, no code path should trigger this.
  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      browser()->profile()));

  if (!params.launch_files.empty()) {
    // SWA browser tests bypass the code in `WebAppPublisherHelper` that fills
    // in `override_url`, so fill it in here, assuming the file handler action
    // URL matches the start URL.
    params.override_url =
        web_app::WebAppProvider::GetForLocalAppsUnchecked(browser()->profile())
            ->registrar_unsafe()
            .GetAppStartUrl(params.app_id);
  }

  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));

  if (wait_for_load) {
    navigation_observer.Wait();
    DCHECK(navigation_observer.last_navigation_succeeded());
  }

  if (out_browser) {
    *out_browser =
        web_contents ? chrome::FindBrowserWithTab(web_contents) : nullptr;
  }

  return web_contents;
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchApp(
    apps::AppLaunchParams&& params,
    Browser** browser) {
  return LaunchApp(std::move(params), /* wait_for_load */ true, browser);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchApp(
    SystemWebAppType type,
    Browser** browser) {
  return LaunchApp(LaunchParamsForApp(type), browser);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchAppWithoutWaiting(
    apps::AppLaunchParams&& params,
    Browser** browser) {
  return LaunchApp(std::move(params), /* wait_for_load */ false, browser);
}

content::WebContents* SystemWebAppBrowserTestBase::LaunchAppWithoutWaiting(
    SystemWebAppType type,
    Browser** browser) {
  return LaunchAppWithoutWaiting(LaunchParamsForApp(type), browser);
}

GURL SystemWebAppBrowserTestBase::GetStartUrl(
    const apps::AppLaunchParams& params) {
  return params.override_url.is_valid()
             ? params.override_url
             : web_app::WebAppProvider::GetForLocalAppsUnchecked(
                   browser()->profile())
                   ->registrar_unsafe()
                   .GetAppStartUrl(params.app_id);
}

GURL SystemWebAppBrowserTestBase::GetStartUrl(SystemWebAppType type) {
  return GetStartUrl(LaunchParamsForApp(type));
}

GURL SystemWebAppBrowserTestBase::GetStartUrl() {
  return GetStartUrl(LaunchParamsForApp(GetAppType()));
}

size_t SystemWebAppBrowserTestBase::GetSystemWebAppBrowserCount(
    SystemWebAppType type) {
  auto* browser_list = BrowserList::GetInstance();
  return base::ranges::count_if(*browser_list, [&](Browser* browser) {
    return ash::IsBrowserForSystemWebApp(browser, type);
  });
}

void SystemWebAppBrowserTestBase::SetSystemWebAppInstallation(
    std::unique_ptr<TestSystemWebAppInstallation> installation) {
  CHECK(!installation_);
  installation_ = std::move(installation);
}

SystemWebAppManagerBrowserTest::SystemWebAppManagerBrowserTest() {
  SetSystemWebAppInstallation(
      TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp());
}

}  // namespace ash
