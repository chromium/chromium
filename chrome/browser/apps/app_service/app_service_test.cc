// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_test.h"

#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps {

AppServiceTest::AppServiceTest() = default;

AppServiceTest::~AppServiceTest() = default;

void AppServiceTest::SetUp(Profile* profile) {
  profile_ = profile;
  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile);
  app_service_proxy_->ReInitializeForTesting(profile);

  // Allow async callbacks to run.
  WaitForAppService();
}

void AppServiceTest::UninstallAllApps(Profile* profile) {
  auto* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  std::vector<apps::mojom::AppPtr> apps;
  app_service_proxy->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        apps::mojom::AppPtr app = apps::mojom::App::New();
        app->app_type = update.AppType();
        app->app_id = update.AppId();
        app->readiness = apps::mojom::Readiness::kUninstalledByUser;
        apps.push_back(app.Clone());
      });
  app_service_proxy->AppRegistryCache().OnApps(
      std::move(apps), apps::mojom::AppType::kUnknown,
      false /* should_notify_initialized */);

  // Allow async callbacks to run.
  WaitForAppService();
}

std::string AppServiceTest::GetAppName(const std::string& app_id) const {
  std::string name;
  if (!app_service_proxy_)
    return name;
  app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [&name](const apps::AppUpdate& update) { name = update.Name(); });
  return name;
}

gfx::ImageSkia AppServiceTest::LoadAppIconBlocking(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int32_t size_hint_in_dip) {
  gfx::ImageSkia image_skia;
  base::RunLoop run_loop;
  base::OnceClosure load_app_icon_callback = run_loop.QuitClosure();

  app_service_proxy_->LoadIcon(
      app_type, app_id, apps::mojom::IconType::kStandard, size_hint_in_dip,
      false /* allow_placeholder_icon */,
      base::BindOnce(
          [](gfx::ImageSkia* icon, base::OnceClosure load_app_icon_callback,
             apps::mojom::IconValuePtr icon_value) {
            DCHECK_EQ(apps::mojom::IconType::kStandard, icon_value->icon_type);
            *icon = icon_value->uncompressed;
            std::move(load_app_icon_callback).Run();
          },
          &image_skia, std::move(load_app_icon_callback)));

  run_loop.Run();
  return image_skia;
}

bool AppServiceTest::AreIconImageEqual(const gfx::ImageSkia& src,
                                       const gfx::ImageSkia& dst) {
  return gfx::test::AreBitmapsEqual(src.GetRepresentation(1.0f).GetBitmap(),
                                    dst.GetRepresentation(1.0f).GetBitmap());
}

void AppServiceTest::WaitForAppService() {
  base::RunLoop().RunUntilIdle();
}

void AppServiceTest::FlushMojoCalls() {
  if (app_service_proxy_) {
    app_service_proxy_->FlushMojoCallsForTesting();
  }
}

}  // namespace apps
