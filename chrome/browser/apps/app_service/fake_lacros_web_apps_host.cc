// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/fake_lacros_web_apps_host.h"

#include <utility>
#include <vector>

#include "base/notreached.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Test push one test app.
void PushOneApp() {
  auto* service = chromeos::LacrosService::Get();

  std::vector<apps::mojom::AppPtr> apps;

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = "abcdefg";
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = "lacros test name";
  app->short_name = "lacros test name";

  app->last_launch_time = base::Time();
  app->install_time = base::Time();

  app->install_source = apps::mojom::InstallSource::kUser;

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->paused = apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  app->show_in_management = apps::mojom::OptionalBool::kTrue;
  apps.push_back(std::move(app));
  service->GetRemote<crosapi::mojom::AppPublisher>()->OnApps(std::move(apps));
}

// Test push one test capability_access.
void PushOneCapabilityAccess() {
  auto* service = chromeos::LacrosService::Get();

  std::vector<apps::mojom::CapabilityAccessPtr> capability_accesses;

  apps::mojom::CapabilityAccessPtr capability_access =
      apps::mojom::CapabilityAccess::New();
  capability_access->app_id = "abcdefg";
  capability_access->camera = apps::mojom::OptionalBool::kTrue;
  capability_access->microphone = apps::mojom::OptionalBool::kFalse;
  capability_accesses.push_back(std::move(capability_access));
  service->GetRemote<crosapi::mojom::AppPublisher>()->OnCapabilityAccesses(
      std::move(capability_accesses));
}

}  // namespace

namespace apps {

FakeLacrosWebAppsHost::FakeLacrosWebAppsHost() {}

FakeLacrosWebAppsHost::~FakeLacrosWebAppsHost() = default;

void FakeLacrosWebAppsHost::Init() {
  auto* service = chromeos::LacrosService::Get();

  if (!service) {
    return;
  }

  if (!service->IsAvailable<crosapi::mojom::AppPublisher>())
    return;

  if (service->init_params()->web_apps_enabled) {
    service->GetRemote<crosapi::mojom::AppPublisher>()->RegisterAppController(
        receiver_.BindNewPipeAndPassRemote());
    PushOneApp();
    PushOneCapabilityAccess();
  }
}

void FakeLacrosWebAppsHost::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  NOTIMPLEMENTED();
}

void FakeLacrosWebAppsHost::PauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void FakeLacrosWebAppsHost::UnpauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void FakeLacrosWebAppsHost::GetMenuModel(const std::string& app_id,
                                         GetMenuModelCallback callback) {
  NOTIMPLEMENTED();
}

void FakeLacrosWebAppsHost::LoadIcon(const std::string& app_id,
                                     apps::mojom::IconKeyPtr icon_key,
                                     apps::mojom::IconType icon_type,
                                     int32_t size_hint_in_dip,
                                     LoadIconCallback callback) {
  NOTIMPLEMENTED();
}

void FakeLacrosWebAppsHost::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}
}  // namespace apps
