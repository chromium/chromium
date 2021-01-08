// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/app_launch_handler.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/restore_data.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace chromeos {
namespace full_restore {

AppLaunchHandler::AppLaunchHandler(Profile* profile) : profile_(profile) {
  // FullRestoreReadHandler reads the full restore data from the full restore
  // data file on a background task runner.
  ::full_restore::FullRestoreReadHandler::GetInstance()->ReadFromFile(
      profile_->GetPath(), base::BindOnce(&AppLaunchHandler::OnGetRestoreData,
                                          weak_ptr_factory_.GetWeakPtr()));
}

AppLaunchHandler::~AppLaunchHandler() = default;

void AppLaunchHandler::OnAppUpdate(const apps::AppUpdate& update) {
  // If the restore flag |should_restore_| is false, or the restore data has not
  // been read yet, or the app is not ready, don't launch the app for the
  // restoration.
  if (!should_restore_ || !restore_data_ || !update.ReadinessChanged() ||
      update.Readiness() != apps::mojom::Readiness::kReady) {
    return;
  }

  // If there is no restore data or the launch list for the app is empty, don't
  // launch the app.
  const auto& app_id_to_launch_list = restore_data_->app_id_to_launch_list();
  if (app_id_to_launch_list.find(update.AppId()) ==
      app_id_to_launch_list.end()) {
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppLaunchHandler::LaunchApp,
                                weak_ptr_factory_.GetWeakPtr(),
                                update.AppType(), update.AppId()));
}

void AppLaunchHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps::AppRegistryCache::Observer::Observe(nullptr);
}

void AppLaunchHandler::SetShouldRestore() {
  should_restore_ = true;
  MaybePostRestore();
}

void AppLaunchHandler::OnGetRestoreData(
    std::unique_ptr<::full_restore::RestoreData> restore_data) {
  restore_data_ = std::move(restore_data);

  // After reading the restore data, the restore data can be cleared from the
  // restore file to save the new restore data.
  ::full_restore::FullRestoreSaveHandler::GetInstance()->Flush(
      profile_->GetPath());

  MaybePostRestore();
}

void AppLaunchHandler::MaybePostRestore() {
  // If the restore flag |should_restore_| is not true, or reading the restore
  // data hasn't finished, don't restore.
  if (!should_restore_ || !restore_data_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppLaunchHandler::MaybeRestore,
                                weak_ptr_factory_.GetWeakPtr()));
}

void AppLaunchHandler::MaybeRestore() {
  // If there is no launch list from the restore data, we don't need to handle
  // the restoration.
  const auto& launch_list = restore_data_->app_id_to_launch_list();
  if (launch_list.empty())
    return;

  // Observe AppRegistryCache to get the notification when the app is ready.
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  auto* cache = &apps::AppServiceProxyFactory::GetForProfile(profile_)
                     ->AppRegistryCache();
  Observe(cache);

  // Add the app to |app_ids| if there is a launch list from the restore data
  // for the app.
  std::set<std::string> app_ids;
  cache->ForEachApp([&app_ids, &launch_list](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::mojom::Readiness::kReady &&
        launch_list.find(update.AppId()) != launch_list.end()) {
      app_ids.insert(update.AppId());
    }
  });

  for (const auto& app_id : app_ids)
    LaunchApp(cache->GetAppType(app_id), app_id);
}

void AppLaunchHandler::LaunchApp(apps::mojom::AppType app_type,
                                 const std::string& app_id) {
  DCHECK(restore_data_);

  // For the Chrome browser, the browser session restore is used to restore the
  // web pages, so we don't need to launch the app.
  if (app_id == extension_misc::kChromeAppId) {
    restore_data_->RemoveApp(app_id);
    return;
  }

  const auto it = restore_data_->app_id_to_launch_list().find(app_id);
  if (it == restore_data_->app_id_to_launch_list().end() ||
      it->second.empty()) {
    restore_data_->RemoveApp(app_id);
    return;
  }

  switch (app_type) {
    case apps::mojom::AppType::kArc:
      // TODO(crbug.com/1146900): Handle ARC apps
      break;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      LaunchWebAppOrExtension(app_id, it->second);
      break;
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kMacOs:
    case apps::mojom::AppType::kLacros:
    case apps::mojom::AppType::kRemote:
    case apps::mojom::AppType::kBorealis:
      NOTREACHED();
      break;
  }
  restore_data_->RemoveApp(app_id);
}

void AppLaunchHandler::LaunchWebAppOrExtension(
    const std::string& app_id,
    const ::full_restore::RestoreData::LaunchList& launch_list) {
  auto* launcher = apps::AppServiceProxyFactory::GetForProfile(profile_)
                       ->BrowserAppLauncher();
  if (!launcher)
    return;

  for (const auto& it : launch_list) {
    DCHECK(it.second->container.has_value());
    DCHECK(it.second->disposition.has_value());
    DCHECK(it.second->display_id.has_value());
    apps::mojom::IntentPtr intent;
    apps::AppLaunchParams params(
        app_id,
        static_cast<apps::mojom::LaunchContainer>(it.second->container.value()),
        static_cast<WindowOpenDisposition>(it.second->disposition.value()),
        it.second->display_id.value(),
        it.second->file_paths.has_value() ? it.second->file_paths.value()
                                          : std::vector<base::FilePath>{},
        it.second->intent.has_value() ? it.second->intent.value() : intent);
    params.restore_id = it.first;
    launcher->LaunchAppWithParams(std::move(params));
  }
}

}  // namespace full_restore
}  // namespace chromeos
