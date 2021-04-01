// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace app_time {

namespace {

// Gets AppId from |update|.
AppId AppIdFromAppUpdate(const apps::AppUpdate& update) {
  bool is_arc = update.AppType() == apps::mojom::AppType::kArc;
  return AppId(update.AppType(),
               is_arc ? update.PublisherId() : update.AppId());
}

// Gets AppId from |update|.
AppId AppIdFromInstanceUpdate(const apps::InstanceUpdate& update,
                              apps::AppRegistryCache* app_cache) {
  AppId app_id(apps::mojom::AppType::kUnknown, update.AppId());

  app_cache->ForOneApp(update.AppId(),
                       [&app_id](const apps::AppUpdate& update) {
                         app_id = AppIdFromAppUpdate(update);
                       });
  return app_id;
}

// Gets app service id from |app_id|.
std::string AppServiceIdFromAppId(const AppId& app_id, Profile* profile) {
  return app_id.app_type() == apps::mojom::AppType::kArc
             ? arc::ArcPackageNameToAppId(app_id.app_id(), profile)
             : app_id.app_id();
}

apps::PauseData PauseAppInfoToPauseData(const PauseAppInfo& pause_info) {
  apps::PauseData details;
  details.should_show_pause_dialog = pause_info.show_pause_dialog;
  details.hours = pause_info.daily_limit.InHours();
  details.minutes = pause_info.daily_limit.InMinutes() % 60;
  return details;
}

}  // namespace

AppServiceWrapper::AppServiceWrapper(Profile* profile) : profile_(profile) {
  apps::AppRegistryCache::Observer::Observe(&GetAppCache());
  apps::InstanceRegistry::Observer::Observe(&GetInstanceRegistry());
}

AppServiceWrapper::~AppServiceWrapper() = default;

void AppServiceWrapper::PauseApp(const PauseAppInfo& pause_app) {
  const std::map<std::string, apps::PauseData> apps{
      {GetAppServiceId(pause_app.app_id), PauseAppInfoToPauseData(pause_app)}};
  GetAppProxy()->PauseApps(apps);
}

void AppServiceWrapper::PauseApps(
    const std::vector<PauseAppInfo>& paused_apps) {
  std::map<std::string, apps::PauseData> apps;
  for (const auto& entry : paused_apps) {
    apps[GetAppServiceId(entry.app_id)] = PauseAppInfoToPauseData(entry);
  }
  GetAppProxy()->PauseApps(apps);
}

void AppServiceWrapper::ResumeApp(const AppId& app_id) {
  const std::set<std::string> apps{GetAppServiceId(app_id)};
  GetAppProxy()->UnpauseApps(apps);
}

void AppServiceWrapper::LaunchApp(const std::string& app_service_id) {
  GetAppProxy()->Launch(app_service_id, ui::EventFlags::EF_NONE,
                        apps::mojom::LaunchSource::kFromParentalControls,
                        apps::MakeWindowInfo(display::kDefaultDisplayId));
}

std::vector<AppId> AppServiceWrapper::GetInstalledApps() const {
  std::vector<AppId> installed_apps;
  GetAppCache().ForEachApp(
      [&installed_apps, this](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kUninstalledByUser)
          return;

        const AppId app_id = AppIdFromAppUpdate(update);
        if (!ShouldIncludeApp(app_id))
          return;

        installed_apps.push_back(app_id);
      });
  return installed_apps;
}

bool AppServiceWrapper::IsHiddenArcApp(const AppId& app_id) const {
  if (app_id.app_type() != apps::mojom::AppType::kArc)
    return false;

  bool is_hidden = false;
  const std::string app_service_id = AppServiceIdFromAppId(app_id, profile_);

  GetAppCache().ForOneApp(
      app_service_id, [&is_hidden](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kUninstalledByUser)
          return;

        is_hidden =
            update.ShowInLauncher() == apps::mojom::OptionalBool::kFalse;
      });

  return is_hidden;
}

std::vector<AppId> AppServiceWrapper::GetHiddenArcApps() const {
  std::vector<AppId> hidden_arc_apps;
  GetAppCache().ForEachApp([&hidden_arc_apps](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::mojom::Readiness::kUninstalledByUser)
      return;

    const AppId app_id = AppIdFromAppUpdate(update);
    if (app_id.app_type() != apps::mojom::AppType::kArc ||
        update.ShowInLauncher() != apps::mojom::OptionalBool::kFalse) {
      return;
    }

    hidden_arc_apps.push_back(app_id);
  });
  return hidden_arc_apps;
}

std::string AppServiceWrapper::GetAppName(const AppId& app_id) const {
  const std::string app_service_id = AppServiceIdFromAppId(app_id, profile_);
  DCHECK(!app_service_id.empty());

  std::string app_name;
  GetAppCache().ForOneApp(
      app_service_id,
      [&app_name](const apps::AppUpdate& update) { app_name = update.Name(); });
  return app_name;
}

void AppServiceWrapper::GetAppIcon(
    const AppId& app_id,
    int size_hint_in_dp,
    base::OnceCallback<void(base::Optional<gfx::ImageSkia>)> on_icon_ready)
    const {
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  const std::string app_service_id = AppServiceIdFromAppId(app_id, profile_);
  DCHECK(!app_service_id.empty());

  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  proxy->LoadIconFromIconKey(
      app_id.app_type(), app_service_id, apps::mojom::IconKey::New(), icon_type,
      size_hint_in_dp,
      /* allow_placeholder_icon */ false,
      base::BindOnce(
          [](base::OnceCallback<void(base::Optional<gfx::ImageSkia>)> callback,
             apps::mojom::IconValuePtr icon_value) {
            auto icon_type = (base::FeatureList::IsEnabled(
                                 features::kAppServiceAdaptiveIcon))
                                 ? apps::mojom::IconType::kStandard
                                 : apps::mojom::IconType::kUncompressed;
            if (!icon_value || icon_value->icon_type != icon_type) {
              std::move(callback).Run(base::nullopt);
            } else {
              std::move(callback).Run(icon_value->uncompressed);
            }
          },
          std::move(on_icon_ready)));
}

std::string AppServiceWrapper::GetAppServiceId(const AppId& app_id) const {
  return AppServiceIdFromAppId(app_id, profile_);
}

bool AppServiceWrapper::IsAppInstalled(const std::string& app_id) {
  return GetAppCache().GetAppType(app_id) != apps::mojom::AppType::kUnknown;
}

AppId AppServiceWrapper::AppIdFromAppServiceId(
    const std::string& app_service_id,
    apps::mojom::AppType app_type) const {
  base::Optional<AppId> app_id;
  GetAppCache().ForOneApp(app_service_id,
                          [&app_id](const apps::AppUpdate& update) {
                            app_id = AppIdFromAppUpdate(update);
                          });
  DCHECK(app_id);
  return *app_id;
}

void AppServiceWrapper::AddObserver(EventListener* listener) {
  DCHECK(listener);
  listeners_.AddObserver(listener);
}

void AppServiceWrapper::RemoveObserver(EventListener* listener) {
  DCHECK(listener);
  listeners_.RemoveObserver(listener);
}

void AppServiceWrapper::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged())
    return;

  const AppId app_id = AppIdFromAppUpdate(update);
  if (!ShouldIncludeApp(app_id))
    return;

  switch (update.Readiness()) {
    case apps::mojom::Readiness::kReady:
      for (auto& listener : listeners_)
        if (update.StateIsNull()) {
          // It is the first update about this app.
          // Note that AppService does not store info between sessions and this
          // will be called at the beginning of every session.
          listener.OnAppInstalled(app_id);
        } else {
          listener.OnAppAvailable(app_id);
        }
      break;
    case apps::mojom::Readiness::kUninstalledByUser:
      for (auto& listener : listeners_)
        listener.OnAppUninstalled(app_id);
      break;
    case apps::mojom::Readiness::kDisabledByUser:
    case apps::mojom::Readiness::kDisabledByPolicy:
    case apps::mojom::Readiness::kDisabledByBlocklist:
      for (auto& listener : listeners_)
        listener.OnAppBlocked(app_id);
      break;
    default:
      break;
  }
}

void AppServiceWrapper::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps::AppRegistryCache::Observer::Observe(nullptr);
}

void AppServiceWrapper::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged())
    return;

  const AppId app_id = AppIdFromInstanceUpdate(update, &GetAppCache());
  if (!ShouldIncludeApp(app_id))
    return;

  bool is_active = update.State() & apps::InstanceState::kActive;
  bool is_destroyed = update.State() & apps::InstanceState::kDestroyed;
  for (auto& listener : listeners_) {
    if (is_active) {
      listener.OnAppActive(app_id, update.Window(), update.LastUpdatedTime());
    } else {
      listener.OnAppInactive(app_id, update.Window(), update.LastUpdatedTime());
    }

    if (is_destroyed) {
      listener.OnAppDestroyed(app_id, update.Window(),
                              update.LastUpdatedTime());
    }
  }
}

void AppServiceWrapper::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  apps::InstanceRegistry::Observer::Observe(nullptr);
}

apps::AppServiceProxyChromeOs* AppServiceWrapper::GetAppProxy() {
  return apps::AppServiceProxyFactory::GetForProfile(profile_);
}

apps::AppRegistryCache& AppServiceWrapper::GetAppCache() const {
  return apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache();
}

apps::InstanceRegistry& AppServiceWrapper::GetInstanceRegistry() const {
  return apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->InstanceRegistry();
}

bool AppServiceWrapper::ShouldIncludeApp(const AppId& app_id) const {
  if (IsHiddenArcApp(app_id))
    return false;

  if (app_id.app_type() == apps::mojom::AppType::kExtension) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
            app_id.app_id(),
            extensions::ExtensionRegistry::IncludeFlag::EVERYTHING);

    // If we are not able to find the extension, return false.
    if (!extension)
      return false;

    // Some preinstalled apps that open in browser window are legacy packaged
    // apps. Example Google Slides app.
    return extension->is_hosted_app() || extension->is_legacy_packaged_app();
  }

  return app_id.app_type() == apps::mojom::AppType::kArc ||
         app_id.app_type() == apps::mojom::AppType::kWeb;
}

}  // namespace app_time
}  // namespace chromeos
