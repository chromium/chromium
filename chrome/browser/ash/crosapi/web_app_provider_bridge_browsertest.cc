// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash_requires_lacros_browsertestbase.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_id.h"
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

  web_app::AppId InstallWebApp(const std::string& start_url,
                               apps::WindowMode mode) {
    base::test::TestFuture<const std::string&> future;
    GetStandaloneBrowserTestController()->InstallWebApp(start_url, mode,
                                                        future.GetCallback());
    auto app_id = future.Take();
    CHECK(!app_id.empty());
    apps::AppReadinessWaiter(profile(), app_id).Await();
    return app_id;
  }

  web_app::AppId InstallSubApp(const web_app::AppId& parent_app_id,
                               std::string sub_app_start_url) {
    base::test::TestFuture<const std::string&> future;
    GetStandaloneBrowserTestController()->InstallSubApp(
        parent_app_id, sub_app_start_url, future.GetCallback());
    auto sub_app_id = future.Take();
    CHECK(!sub_app_id.empty());
    apps::AppReadinessWaiter(profile(), sub_app_id).Await();
    return sub_app_id;
  }

  Profile* profile() { return browser()->profile(); }
};

IN_PROC_BROWSER_TEST_F(WebAppProviderBridgeBrowserTest, GetSubAppIds) {
  web_app::AppId parent_app_id =
      InstallWebApp("https://www.parent-app.com", apps::WindowMode::kWindow);
  web_app::AppId sub_app_id_1 =
      InstallSubApp(parent_app_id, "https://www.parent-app.com/sub-app-1");
  web_app::AppId sub_app_id_2 =
      InstallSubApp(parent_app_id, "https://www.parent-app.com/sub-app-2");

  base::flat_set<web_app::AppId> expected;
  expected.emplace(sub_app_id_1);
  expected.emplace(sub_app_id_2);

  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  ASSERT_TRUE(web_app_provider_bridge);

  base::test::TestFuture<const std::vector<web_app::AppId>&>
      get_sub_apps_future;

  web_app_provider_bridge->GetSubAppIds(parent_app_id,
                                        get_sub_apps_future.GetCallback());

  base::flat_set<web_app::AppId> results_set{get_sub_apps_future.Get()};
  EXPECT_EQ(2u, results_set.size());
  EXPECT_EQ(results_set, expected);
}

IN_PROC_BROWSER_TEST_F(WebAppProviderBridgeBrowserTest, GetSubAppToParentMap) {
  web_app::AppId parent_app_id =
      InstallWebApp("https://www.parent-app.com", apps::WindowMode::kWindow);
  web_app::AppId sub_app_id_1 =
      InstallSubApp(parent_app_id, "https://www.parent-app.com/sub-app-1");
  web_app::AppId sub_app_id_2 =
      InstallSubApp(parent_app_id, "https://www.parent-app.com/sub-app-2");
  // This app should not appear at all in the result map.
  web_app::AppId unrelated_app_id =
      InstallWebApp("https://www.unrelated-app.com", apps::WindowMode::kWindow);

  base::flat_map<web_app::AppId, web_app::AppId> expected;
  expected[sub_app_id_1] = parent_app_id;
  expected[sub_app_id_2] = parent_app_id;

  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  ASSERT_TRUE(web_app_provider_bridge);

  base::test::TestFuture<const base::flat_map<web_app::AppId, web_app::AppId>&>
      get_sub_apps_to_parent_map_future;

  web_app_provider_bridge->GetSubAppToParentMap(
      get_sub_apps_to_parent_map_future.GetCallback());

  base::flat_map<web_app::AppId, web_app::AppId> results{
      get_sub_apps_to_parent_map_future.Get()};
  EXPECT_EQ(2u, results.size());
  EXPECT_EQ(results, expected);
}
}  // namespace
}  // namespace crosapi
