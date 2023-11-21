// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_package_install_priority_handler.h"

#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

ArcPackageInstallPriorityHandler::ArcPackageInstallPriorityHandler(
    Profile* profile)
    : profile_(profile) {}

ArcPackageInstallPriorityHandler::~ArcPackageInstallPriorityHandler() {}

void ArcPackageInstallPriorityHandler::Shutdown() {
  profile_ = nullptr;
}

void ArcPackageInstallPriorityHandler::PromotePackageInstall(
    const std::string& package_name) {
  const auto synced_package = synced_pacakge_priority_map_.find(package_name);
  if (synced_package != synced_pacakge_priority_map_.end()) {
    if (synced_package->second == arc::mojom::InstallPriority::kLow) {
      InstallSyncedPacakge(package_name, arc::mojom::InstallPriority::kMedium);
    }
    // Package already has highest applicable install priority. Do nothing.
    return;
  }
  // TODO(lgcheng) handles fast app reinstall apps.
}

void ArcPackageInstallPriorityHandler::ClearPackage(
    const std::string& package_name) {
  synced_pacakge_priority_map_.erase(package_name);
}

void ArcPackageInstallPriorityHandler::Clear() {
  synced_pacakge_priority_map_.clear();
}

void ArcPackageInstallPriorityHandler::InstallSyncedPacakge(
    const std::string& package_name,
    arc::mojom::InstallPriority priority) {
  auto* arc_package_sync_service = ArcPackageSyncableService::Get(profile_);
  DCHECK(arc_package_sync_service);

  arc_package_sync_service->InstallPendingPackage(package_name, priority);

  synced_pacakge_priority_map_[package_name] = priority;
}

arc::mojom::InstallPriority
ArcPackageInstallPriorityHandler::GetInstallPriorityForTesting(
    const std::string& package_name) const {
  const auto sync_iter = synced_pacakge_priority_map_.find(package_name);
  if (sync_iter != synced_pacakge_priority_map_.end()) {
    return sync_iter->second;
  }

  return arc::mojom::InstallPriority::kUndefined;
}

}  // namespace arc
