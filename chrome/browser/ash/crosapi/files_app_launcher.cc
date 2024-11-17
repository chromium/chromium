// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/files_app_launcher.h"

#include <utility>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/check.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chromeos/ash/components/file_manager/app_id.h"

namespace crosapi {

FilesAppLauncher::FilesAppLauncher(apps::AppServiceProxy* proxy)
    : proxy_(proxy) {}

FilesAppLauncher::~FilesAppLauncher() = default;

void FilesAppLauncher::Launch(base::OnceClosure callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  auto& instance_registry = proxy_->InstanceRegistry();
  if (instance_registry.ContainsAppId(file_manager::kFileManagerSwaAppId)) {
    std::move(callback).Run();
    return;
  }

  callback_ = std::move(callback);

  auto& app_registry = proxy_->AppRegistryCache();
  bool is_ready = false;
  app_registry.ForOneApp(file_manager::kFileManagerSwaAppId,
                         [&is_ready](const apps::AppUpdate& update) {
                           is_ready =
                               update.Readiness() == apps::Readiness::kReady;
                         });
  if (is_ready) {
    // Files.app is already ready, so launch it.
    LaunchInternal();
    return;
  }

  // Files.app is not yet initialized. Wait for its ready.
  app_registry_cache_observer_.Observe(&app_registry);
}

void FilesAppLauncher::LaunchInternal() {
  // Start observing the launching.
  instance_registry_observation_.Observe(&proxy_->InstanceRegistry());

  // Launching traditional files.app and launching SWA files.app need quite
  // different procedure.
  // Use ash::NewWindowDelegate::OpenFileManager that handles the diff nicely.
  ash::NewWindowDelegate::GetPrimary()->OpenFileManager();
}

void FilesAppLauncher::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != file_manager::kFileManagerSwaAppId)
    return;

  if (update.Readiness() != apps::Readiness::kReady)
    return;

  // So it's ready to launch files.app now.
  // We no longer need to observe the update.
  app_registry_cache_observer_.Reset();

  LaunchInternal();
}

void FilesAppLauncher::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void FilesAppLauncher::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (update.AppId() != file_manager::kFileManagerSwaAppId)
    return;

  // So launching is progressed. Stop observing and run the callback
  // to notify the caller of Launch().
  instance_registry_observation_.Reset();
  std::move(callback_).Run();
}

void FilesAppLauncher::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  instance_registry_observation_.Reset();
}

}  // namespace crosapi
