// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace crosapi {
namespace {
class WebAppProviderBridgeBrowserTest
    : public AshRequiresLacrosBrowserTestBase {
 protected:
  void SetUp() override {
    if (!HasLacrosArgument()) {
      GTEST_SKIP() << "Skipping test class because Lacros is not enabled";
    }
    AshRequiresLacrosBrowserTestBase::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    AshRequiresLacrosBrowserTestBase::SetUpInProcessBrowserTestFixture();
    EnableFeaturesInLacros(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode});
  }

  webapps::AppId InstallIsolatedWebApp(
      const std::string& relative_test_data_dir) {
    auto server = web_app::CreateAndStartDevServer(relative_test_data_dir);
    base::test::TestFuture<mojom::InstallWebAppResultPtr> future;
    GetStandaloneBrowserTestController()->InstallIsolatedWebApp(
        crosapi::mojom::IsolatedWebAppLocation::NewProxyOrigin(
            server->base_url()),
        /*dev_mode=*/true, future.GetCallback());
    mojom::InstallWebAppResultPtr install_restult = future.Take();

    CHECK(install_restult->is_app_id())
        << "Isolated web app installation failed with error: "
        << install_restult->get_error_message();
    webapps::AppId app_id = install_restult->get_app_id();
    apps::AppReadinessWaiter(GetAshProfile(), app_id).Await();
    return app_id;
  }

  webapps::AppId InstallSubApp(const webapps::AppId& parent_app_id,
                               std::string sub_app_start_url) {
    base::test::TestFuture<const std::string&> future;
    GetStandaloneBrowserTestController()->InstallSubApp(
        parent_app_id, sub_app_start_url, future.GetCallback());
    auto sub_app_id = future.Take();
    CHECK(!sub_app_id.empty());
    apps::AppReadinessWaiter(GetAshProfile(), sub_app_id).Await();
    return sub_app_id;
  }
};

IN_PROC_BROWSER_TEST_F(WebAppProviderBridgeBrowserTest, GetSubAppIds) {
  webapps::AppId parent_app_id =
      InstallIsolatedWebApp("web_apps/subapps_isolated_app");

  webapps::AppId sub_app_id_1 = InstallSubApp(parent_app_id, "/sub1/page.html");
  webapps::AppId sub_app_id_2 = InstallSubApp(parent_app_id, "/sub2/page.html");

  base::flat_set<webapps::AppId> expected;
  expected.emplace(sub_app_id_1);
  expected.emplace(sub_app_id_2);

  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  ASSERT_TRUE(web_app_provider_bridge);

  base::test::TestFuture<const std::vector<webapps::AppId>&>
      get_sub_apps_future;

  web_app_provider_bridge->GetSubAppIds(parent_app_id,
                                        get_sub_apps_future.GetCallback());

  base::flat_set<webapps::AppId> results_set{get_sub_apps_future.Get()};
  EXPECT_EQ(2u, results_set.size());
  EXPECT_EQ(results_set, expected);
}

IN_PROC_BROWSER_TEST_F(WebAppProviderBridgeBrowserTest, GetSubAppToParentMap) {
  webapps::AppId parent_app_id =
      InstallIsolatedWebApp("web_apps/subapps_isolated_app");

  webapps::AppId sub_app_id_1 = InstallSubApp(parent_app_id, "/sub1/page.html");
  webapps::AppId sub_app_id_2 = InstallSubApp(parent_app_id, "/sub2/page.html");
  // This app should not appear at all in the result map.
  webapps::AppId unrelated_app_id =
      InstallIsolatedWebApp("web_apps/simple_isolated_app");

  base::flat_map<webapps::AppId, webapps::AppId> expected;
  expected[sub_app_id_1] = parent_app_id;
  expected[sub_app_id_2] = parent_app_id;

  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  ASSERT_TRUE(web_app_provider_bridge);

  base::test::TestFuture<const base::flat_map<webapps::AppId, webapps::AppId>&>
      get_sub_apps_to_parent_map_future;

  web_app_provider_bridge->GetSubAppToParentMap(
      get_sub_apps_to_parent_map_future.GetCallback());

  base::flat_map<webapps::AppId, webapps::AppId> results{
      get_sub_apps_to_parent_map_future.Get()};
  EXPECT_EQ(2u, results.size());
  EXPECT_EQ(results, expected);
}
}  // namespace
}  // namespace crosapi
