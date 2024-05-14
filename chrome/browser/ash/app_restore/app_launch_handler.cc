// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_launch_handler.h"

#include <utility>
#include <vector>

#include "apps/launcher.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace ash {

namespace {

// Returns apps::AppTypeName used for metrics.
apps::AppTypeName GetHistogrameAppType(apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kUnknown:
      return apps::AppTypeName::kUnknown;
    case apps::AppType::kArc:
      return apps::AppTypeName::kArc;
    case apps::AppType::kBuiltIn:
    case apps::AppType::kCrostini:
      return apps::AppTypeName::kUnknown;
    case apps::AppType::kChromeApp:
      return apps::AppTypeName::kChromeApp;
    case apps::AppType::kWeb:
      return apps::AppTypeName::kWeb;
    case apps::AppType::kPluginVm:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
      return apps::AppTypeName::kUnknown;
    case apps::AppType::kSystemWeb:
      return apps::AppTypeName::kSystemWeb;
  }
}

}  // namespace

AppLaunchHandler::AppLaunchHandler(Profile* profile) : profile_(profile) {}

AppLaunchHandler::~AppLaunchHandler() = default;

bool AppLaunchHandler::HasRestoreData() const {
  return restore_data_ && !restore_data_->app_id_to_launch_list().empty();
}

void AppLaunchHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == app_constants::kChromeAppId || !restore_data_ ||
      !update.ReadinessChanged()) {
    return;
  }

  if (!apps_util::IsInstalled(update.Readiness())) {
    restore_data_->RemoveApp(update.AppId());
    return;
  }

  // If the app is not ready, don't launch the app for the restoration.
  if (update.Readiness() != apps::Readiness::kReady)
    return;

  // If there is no restore data or the launch list for the app is empty, don't
  // launch the app.
  const auto& app_id_to_launch_list = restore_data_->app_id_to_launch_list();
  if (app_id_to_launch_list.find(update.AppId()) ==
      app_id_to_launch_list.end()) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&AppLaunchHandler::LaunchApp, GetWeakPtrAppLaunchHandler(),
                     update.AppType(), update.AppId()));
}

void AppLaunchHandler::OnAppTypeInitialized(apps::AppType app_type) {
  // Do nothing: overridden by subclasses.
}

void AppLaunchHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppLaunchHandler::LaunchApps() {
  // If there is no launch list from the restore data, we don't need to handle
  // launching.
  const auto& launch_list = restore_data_->app_id_to_launch_list();
  if (launch_list.empty())
    return;

  // Observe AppRegistryCache to get the notification when the app is ready.
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  auto* cache = &apps::AppServiceProxyFactory::GetForProfile(profile_)
                     ->AppRegistryCache();
  ObserveCache(cache);
  for (const auto app_type : cache->InitializedAppTypes()) {
    OnAppTypeInitialized(app_type);
  }

  // Add the app to `app_ids` if there is a launch list from the restore data
  // for the app.
  std::set<std::string> app_ids;
  cache->ForEachApp([&app_ids, &launch_list](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::Readiness::kReady &&
        launch_list.find(update.AppId()) != launch_list.end()) {
      app_ids.insert(update.AppId());
    }
  });

  for (const auto& app_id : app_ids) {
    // Chrome browser web pages are restored separately, so we don't need to
    // launch browser windows.
    if (app_id == app_constants::kChromeAppId)
      continue;

    LaunchApp(cache->GetAppType(app_id), app_id);
  }
}

bool AppLaunchHandler::ShouldLaunchSystemWebAppOrChromeApp(
    const std::string& app_id,
    const ::app_restore::RestoreData::LaunchList& launch_list) {
  return true;
}

void AppLaunchHandler::LaunchApp(apps::AppType app_type,
                                 const std::string& app_id) {
  DCHECK(restore_data_);
  DCHECK_NE(app_id, app_constants::kChromeAppId);

  const auto it = restore_data_->app_id_to_launch_list().find(app_id);
  if (it == restore_data_->app_id_to_launch_list().end() ||
      it->second.empty()) {
    restore_data_->RemoveApp(app_id);
    return;
  }

  switch (app_type) {
    case apps::AppType::kArc:
      // ArcAppLaunchHandler handles ARC apps restoration and ARC apps
      // restoration could be delayed, so return to preserve the restore data
      // for ARC apps.
      return;
    case apps::AppType::kChromeApp:
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
    case apps::AppType::kStandaloneBrowserChromeApp:
      if (ShouldLaunchSystemWebAppOrChromeApp(app_id, it->second))
        LaunchSystemWebAppOrChromeApp(app_type, app_id, it->second);
      break;
    case apps::AppType::kStandaloneBrowser:
      // For Lacros, we can't use the AppService launch interface to restore,
      // but call Lacros interface to restore with session restore.
      return;
    case apps::AppType::kBuiltIn:
    case apps::AppType::kCrostini:
    case apps::AppType::kPluginVm:
    case apps::AppType::kUnknown:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  restore_data_->RemoveApp(app_id);
}

void AppLaunchHandler::ObserveCache(apps::AppRegistryCache* source) {
  DCHECK(source);
  if (!app_registry_cache_observer_.IsObservingSource(source)) {
    app_registry_cache_observer_.Reset();
    app_registry_cache_observer_.Observe(source);
  }
}

void AppLaunchHandler::LaunchSystemWebAppOrChromeApp(
    apps::AppType app_type,
    const std::string& app_id,
    const app_restore::RestoreData::LaunchList& launch_list) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);

  if (app_type == apps::AppType::kChromeApp ||
      app_type == apps::AppType::kStandaloneBrowserChromeApp) {
    OnExtensionLaunching(app_id);
  }

  for (const auto& it : launch_list) {
    RecordRestoredAppLaunch(GetHistogrameAppType(app_type));

    if (it.second->handler_id.has_value()) {
      const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
              app_id);
      if (extension) {
        DCHECK(!it.second->file_paths.empty());
        apps::LaunchPlatformAppWithFileHandler(profile_, extension,
                                               it.second->handler_id.value(),
                                               it.second->file_paths);
      }
      continue;
    }

    // Desk templates may have partial data. See http://crbug/1232520
    if (!it.second->container.has_value() ||
        !it.second->disposition.has_value() ||
        !it.second->display_id.has_value()) {
      continue;
    }

    apps::AppLaunchParams params(
        app_id,
        static_cast<apps::LaunchContainer>(it.second->container.value()),
        static_cast<WindowOpenDisposition>(it.second->disposition.value()),
        it.second->override_url.value_or(GURL()),
        apps::LaunchSource::kFromFullRestore, it.second->display_id.value(),
        it.second->file_paths,
        it.second->intent ? it.second->intent->Clone() : nullptr);
    params.restore_id = it.first;
    proxy->LaunchAppWithParams(std::move(params));
  }
}

}  // namespace ash
