// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_test.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "components/services/app_service/public/cpp/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps {

AppServiceTest::AppServiceTest() = default;

AppServiceTest::~AppServiceTest() = default;

void AppServiceTest::SetUp(Profile* profile) {
  app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile);
  app_service_proxy_->ReinitializeForTesting(profile);
  WaitForAppServiceProxyReady(app_service_proxy_);
}

void AppServiceTest::UninstallAllApps(Profile* profile) {
  auto* app_service_proxy = AppServiceProxyFactory::GetForProfile(profile);
  std::vector<AppPtr> apps;
  app_service_proxy->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        AppPtr app = std::make_unique<App>(update.AppType(), update.AppId());
        app->readiness = Readiness::kUninstalledByUser;
        apps.push_back(std::move(app));
      });
  app_service_proxy->OnApps(std::move(apps), AppType::kUnknown,
                            false /* should_notify_initialized */);
}

std::string AppServiceTest::GetAppName(const std::string& app_id) const {
  std::string name;
  if (!app_service_proxy_) {
    return name;
  }
  app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [&name](const AppUpdate& update) { name = update.Name(); });
  return name;
}

gfx::ImageSkia AppServiceTest::LoadAppIconBlocking(const std::string& app_id,
                                                   int32_t size_hint_in_dip) {
  base::test::TestFuture<apps::IconValuePtr> future;
  app_service_proxy_->LoadIcon(app_id, IconType::kStandard, size_hint_in_dip,
                               /*allow_placeholder_icon=*/false,
                               future.GetCallback());

  auto icon = future.Take();
  DCHECK_EQ(IconType::kStandard, icon->icon_type);
  return icon->uncompressed;
}

bool AppServiceTest::AreIconImageEqual(const gfx::ImageSkia& src,
                                       const gfx::ImageSkia& dst) {
  return gfx::test::AreBitmapsEqual(src.GetRepresentation(1.0f).GetBitmap(),
                                    dst.GetRepresentation(1.0f).GetBitmap());
}

void WaitForAppServiceProxyReady(AppServiceProxy* proxy) {
  // TODO(b/329521029): Remove once all tests that used this are reverted.
}

}  // namespace apps
