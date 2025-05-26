// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <set>
#include <vector>

#include "base/check_deref.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CurrentProfile;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

apps::AppRegistryCache& GetAppServiceRegistryCache() {
  apps::AppServiceProxy& app_service = CHECK_DEREF(
      apps::AppServiceProxyFactory::GetForProfile(&CurrentProfile()));
  return app_service.AppRegistryCache();
}

web_app::WebAppProvider* GetWebAppProvider() {
  return web_app::WebAppProvider::GetForWebApps(&CurrentProfile());
}

bool IsKioskAppInstalledInAppService(const KioskApp& app,
                                     const apps::AppRegistryCache& registry) {
  switch (app.id().type) {
    case KioskAppType::kChromeApp:
      return registry.IsAppInstalled(app.id().app_id.value());
    case KioskAppType::kWebApp:
      return registry.IsAppInstalled(
          web_app::GenerateAppId(std::nullopt, app.url().value()));
    case KioskAppType::kIsolatedWebApp:
    case KioskAppType::kArcvmApp:
      // TODO(crbug.com/379633748): Support IWA in KioskMixin.
      // TODO(crbug.com/419356153): Support ARCVM in KioskMixin.
      NOTREACHED();
  }
}

std::set<apps::AppType> ExpectedInitializedAppTypes(
    ash::KioskAppType kiosk_app_type) {
  std::set<apps::AppType> result{apps::AppType::kChromeApp,
                                 apps::AppType::kExtension};

  // Web apps are not initialized in ChromeApp kiosk.
  if (kiosk_app_type != KioskAppType::kChromeApp) {
    result.emplace(apps::AppType::kWeb);
  }

  return result;
}

}  // namespace

class KioskInstalledAppsTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskInstalledAppsTest() = default;
  KioskInstalledAppsTest(const KioskInstalledAppsTest&) = delete;
  KioskInstalledAppsTest& operator=(const KioskInstalledAppsTest&) = delete;

  ~KioskInstalledAppsTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

 private:
  KioskMixin kiosk_{&mixin_host_, GetParam()};
};

IN_PROC_BROWSER_TEST_P(KioskInstalledAppsTest, InstallsNoAdditionalApps) {
  apps::AppRegistryCache& registry_cache = GetAppServiceRegistryCache();
  EXPECT_EQ(registry_cache.GetAllApps().size(), 1U);
  EXPECT_TRUE(IsKioskAppInstalledInAppService(TheKioskApp(), registry_cache));
  EXPECT_EQ(registry_cache.InitializedAppTypes(),
            ExpectedInitializedAppTypes(TheKioskApp().id().type));

  if (auto* provider = GetWebAppProvider();
      TheKioskApp().id().type == KioskAppType::kChromeApp) {
    // No `WebAppProvider` in Chrome app kiosk as Web apps are not initialized.
    EXPECT_EQ(provider, nullptr);
  } else {
    ASSERT_NE(provider, nullptr);
    const web_app::WebAppRegistrar& web_app_registrar =
        provider->registrar_unsafe();
    EXPECT_EQ(web_app_registrar.GetAppIds().size(), 1U);
    EXPECT_NE(web_app_registrar.GetAppByStartUrl(TheKioskApp().url().value()),
              nullptr);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskInstalledAppsTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash
