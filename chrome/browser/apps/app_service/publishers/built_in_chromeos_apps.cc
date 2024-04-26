// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/built_in_chromeos_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

apps::AppPtr CreateApp(const app_list::InternalApp& internal_app) {
  if ((internal_app.app_id == nullptr) ||
      (internal_app.name_string_resource_id == 0) ||
      (internal_app.icon_resource_id <= 0)) {
    return nullptr;
  }

  auto app = apps::AppPublisher::MakeApp(
      apps::AppType::kBuiltIn, internal_app.app_id, apps::Readiness::kReady,
      l10n_util::GetStringUTF8(internal_app.name_string_resource_id),
      apps::InstallReason::kSystem, apps::InstallSource::kSystem);

  if (internal_app.searchable_string_resource_id != 0) {
    app->additional_search_terms.push_back(
        l10n_util::GetStringUTF8(internal_app.searchable_string_resource_id));
  }

  app->icon_key =
      apps::IconKey(internal_app.icon_resource_id, apps::IconEffects::kNone);

  app->recommendable = internal_app.recommendable;
  app->searchable = internal_app.searchable;
  app->show_in_launcher = internal_app.show_in_launcher;
  app->show_in_shelf = app->show_in_search = internal_app.searchable;
  app->show_in_management = false;
  app->handles_intents = app->show_in_launcher;
  app->allow_uninstall = false;

  return app;
}

}  // namespace

namespace apps {

BuiltInChromeOsApps::BuiltInChromeOsApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {}

BuiltInChromeOsApps::~BuiltInChromeOsApps() = default;

void BuiltInChromeOsApps::Initialize() {
  RegisterPublisher(AppType::kBuiltIn);

  std::vector<AppPtr> apps;
  for (const auto& internal_app : app_list::GetInternalAppList(profile_)) {
    AppPtr app = CreateApp(internal_app);
    if (app) {
      apps.push_back(std::move(app));
    }
  }
  AppPublisher::Publish(std::move(apps), AppType::kBuiltIn,
                        /*should_notify_initialized=*/true);
}

void BuiltInChromeOsApps::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 LaunchSource launch_source,
                                 WindowInfoPtr window_info) {
  // TODO(longbowei): Remove BuiltInChromeOsApps code if no longer needed.
  NOTIMPLEMENTED();
}

void BuiltInChromeOsApps::LaunchAppWithParams(AppLaunchParams&& params,
                                              LaunchCallback callback) {
  Launch(params.app_id, ui::EF_NONE, LaunchSource::kUnknown, nullptr);

  // TODO(crbug.com/40787924): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void BuiltInChromeOsApps::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  MenuItems menu_items;

  if (ShouldAddOpenItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

}  // namespace apps
