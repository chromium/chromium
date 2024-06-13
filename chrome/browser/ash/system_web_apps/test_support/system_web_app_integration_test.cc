// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace ash {

SystemWebAppIntegrationTest::SystemWebAppIntegrationTest() = default;

SystemWebAppIntegrationTest::~SystemWebAppIntegrationTest() = default;

Profile* SystemWebAppIntegrationTest::profile() {
  return browser()->profile();
}

void SystemWebAppIntegrationTest::ExpectSystemWebAppValid(
    ash::SystemWebAppType app_type,
    const GURL& url,
    const std::string& title) {
  WaitForTestSystemAppInstall();

  // Launch but don't wait for page load here because we want to check the
  // browser window's title is set before the page loads.
  // TODO(crbug.com/40140789): This isn't a strong guarantee that we check the
  // title before the page loads. We should improve this.
  Browser* app_browser;
  LaunchAppWithoutWaiting(app_type, &app_browser);

  webapps::AppId app_id = app_browser->app_controller()->app_id();
  EXPECT_EQ(GetManager().GetAppIdForSystemApp(app_type), app_id);
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForTest(profile())->registrar_unsafe();
  EXPECT_EQ(title, registrar.GetAppShortName(app_id));
  EXPECT_EQ(base::ASCIIToUTF16(title),
            app_browser->window()->GetNativeWindow()->GetTitle());
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, web_app::ExternalInstallSource::kSystemInstalled));

  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // The opened window should be showing the url with attached WebUI.
  EXPECT_EQ(url, web_contents->GetVisibleURL());

  content::TestNavigationObserver observer(web_contents);
  observer.WaitForNavigationFinished();
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  content::WebUI* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);
  EXPECT_TRUE(web_ui->GetController());

  // A completed navigation could change the window title. Check again.
  EXPECT_EQ(base::ASCIIToUTF16(title),
            app_browser->window()->GetNativeWindow()->GetTitle());
}

content::WebContents* SystemWebAppIntegrationTest::LaunchAppWithFile(
    ash::SystemWebAppType type,
    const base::FilePath& file_path) {
  apps::AppLaunchParams params = LaunchParamsForApp(type);
  params.launch_files.push_back(file_path);
  return LaunchApp(std::move(params));
}

content::WebContents*
SystemWebAppIntegrationTest::LaunchAppWithFileWithoutWaiting(
    ash::SystemWebAppType type,
    const base::FilePath& file_path) {
  apps::AppLaunchParams params = LaunchParamsForApp(type);
  params.launch_files.push_back(file_path);
  return LaunchAppWithoutWaiting(std::move(params));
}

}  // namespace ash
