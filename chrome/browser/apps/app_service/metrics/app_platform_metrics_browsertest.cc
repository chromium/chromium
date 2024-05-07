// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"

#include <memory>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

using apps::GetAppTypeName;
using webapps::AppId;

class AppPlatformMetricsBrowserTest : public InProcessBrowserTest {
 public:
  AppId InstallWebApp(const GURL& start_url,
                      blink::mojom::DisplayMode display_mode,
                      web_app::mojom::UserDisplayMode user_display_mode) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = display_mode;
    web_app_info->user_display_mode = user_display_mode;
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  AppId InstallSystemWebApp() {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();

    return *ash::GetAppIdForSystemWebApp(profile(),
                                         ash::SystemWebAppType::HELP);
  }

  apps::AppTypeName GetWebAppTypeName(const std::string& app_id,
                                      apps::LaunchContainer container) {
    return GetAppTypeName(profile(), apps::AppType::kWeb, app_id, container);
  }

  apps::AppTypeName GetSystemWebAppTypeName(const std::string& app_id,
                                            apps::LaunchContainer container) {
    return GetAppTypeName(profile(), apps::AppType::kSystemWeb, app_id,
                          container);
  }

  Profile* profile() { return browser()->profile(); }
};

IN_PROC_BROWSER_TEST_F(AppPlatformMetricsBrowserTest, SystemWebApp) {
  const AppId system_app_id = InstallSystemWebApp();

  EXPECT_EQ(GetWebAppTypeName(system_app_id,
                              apps::LaunchContainer::kLaunchContainerWindow),
            apps::AppTypeName::kSystemWeb);

  EXPECT_EQ(GetSystemWebAppTypeName(
                system_app_id, apps::LaunchContainer::kLaunchContainerWindow),
            apps::AppTypeName::kSystemWeb);

  EXPECT_EQ(GetWebAppTypeName(system_app_id,
                              apps::LaunchContainer::kLaunchContainerTab),
            apps::AppTypeName::kSystemWeb);

  EXPECT_EQ(GetSystemWebAppTypeName(system_app_id,
                                    apps::LaunchContainer::kLaunchContainerTab),
            apps::AppTypeName::kSystemWeb);

  EXPECT_EQ(GetWebAppTypeName(system_app_id,
                              apps::LaunchContainer::kLaunchContainerNone),
            apps::AppTypeName::kSystemWeb);

  EXPECT_EQ(GetSystemWebAppTypeName(
                system_app_id, apps::LaunchContainer::kLaunchContainerNone),
            apps::AppTypeName::kSystemWeb);
}

IN_PROC_BROWSER_TEST_F(AppPlatformMetricsBrowserTest, UnknownWebApp) {
  const AppId unknown_app_id = "unknown";

  EXPECT_EQ(GetWebAppTypeName(unknown_app_id,
                              apps::LaunchContainer::kLaunchContainerWindow),
            apps::AppTypeName::kChromeBrowser);

  EXPECT_EQ(GetWebAppTypeName(unknown_app_id,
                              apps::LaunchContainer::kLaunchContainerTab),
            apps::AppTypeName::kChromeBrowser);

  EXPECT_EQ(GetWebAppTypeName(unknown_app_id,
                              apps::LaunchContainer::kLaunchContainerNone),
            apps::AppTypeName::kChromeBrowser);
}

IN_PROC_BROWSER_TEST_F(AppPlatformMetricsBrowserTest, WindowedWebApps) {
  const AppId standalone_app_id = InstallWebApp(
      GURL("https://standalone.example.com/"),
      blink::mojom::DisplayMode::kStandalone,
      /*user_display_mode=*/web_app::mojom::UserDisplayMode::kStandalone);
  const AppId browser_app_id = InstallWebApp(
      GURL("https://browser.example.com/"), blink::mojom::DisplayMode::kBrowser,
      /*user_display_mode=*/web_app::mojom::UserDisplayMode::kStandalone);

  // When container is specified, |user_display_mode| and |display_mode| are
  // ignored.

  EXPECT_EQ(GetWebAppTypeName(standalone_app_id,
                              apps::LaunchContainer::kLaunchContainerWindow),
            apps::AppTypeName::kWeb);

  EXPECT_EQ(GetWebAppTypeName(browser_app_id,
                              apps::LaunchContainer::kLaunchContainerWindow),
            apps::AppTypeName::kWeb);

  EXPECT_EQ(GetWebAppTypeName(standalone_app_id,
                              apps::LaunchContainer::kLaunchContainerTab),
            apps::AppTypeName::kChromeBrowser);

  EXPECT_EQ(GetWebAppTypeName(browser_app_id,
                              apps::LaunchContainer::kLaunchContainerTab),
            apps::AppTypeName::kChromeBrowser);

  // For a web app with no container given, |user_display_mode| kStandalone
  // leads to |AppTypeName::kWeb|.

  EXPECT_EQ(GetWebAppTypeName(standalone_app_id,
                              apps::LaunchContainer::kLaunchContainerNone),
            apps::AppTypeName::kWeb);

  EXPECT_EQ(GetWebAppTypeName(browser_app_id,
                              apps::LaunchContainer::kLaunchContainerNone),
            apps::AppTypeName::kWeb);
}

IN_PROC_BROWSER_TEST_F(AppPlatformMetricsBrowserTest, TabbedWebApps) {
  const AppId standalone_app_id = InstallWebApp(
      GURL("https://standalone.example.com/"),
      blink::mojom::DisplayMode::kStandalone,
      /*user_display_mode=*/web_app::mojom::UserDisplayMode::kBrowser);
  const AppId browser_app_id = InstallWebApp(
      GURL("https://browser.example.com/"), blink::mojom::DisplayMode::kBrowser,
      /*user_display_mode=*/web_app::mojom::UserDisplayMode::kBrowser);

  // When container is specified, |user_display_mode| and |display_mode| are
  // ignored.

  EXPECT_EQ(GetWebAppTypeName(standalone_app_id,
                              apps::LaunchContainer::kLaunchContainerWindow),
            apps::AppTypeName::kWeb);

  EXPECT_EQ(GetWebAppTypeName(browser_app_id,
                              apps::LaunchContainer::kLaunchContainerWindow),
            apps::AppTypeName::kWeb);

  EXPECT_EQ(GetWebAppTypeName(standalone_app_id,
                              apps::LaunchContainer::kLaunchContainerTab),
            apps::AppTypeName::kChromeBrowser);

  EXPECT_EQ(GetWebAppTypeName(browser_app_id,
                              apps::LaunchContainer::kLaunchContainerTab),
            apps::AppTypeName::kChromeBrowser);

  // For a web app with no container given, |user_display_mode| kBrowser leads
  // to |AppTypeName::kChromeBrowser|.

  EXPECT_EQ(GetWebAppTypeName(standalone_app_id,
                              apps::LaunchContainer::kLaunchContainerNone),
            apps::AppTypeName::kChromeBrowser);

  EXPECT_EQ(GetWebAppTypeName(browser_app_id,
                              apps::LaunchContainer::kLaunchContainerNone),
            apps::AppTypeName::kChromeBrowser);
}
