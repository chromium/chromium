// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/arc_app_installer.h"

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace apps {
namespace {
void RecordInstallResultMetric(AppInstallSurface surface,
                               ArcAppInstallResult result) {
  base::UmaHistogramEnumeration(
      "Apps.AppInstallService.ArcAppInstaller.InstallResult", result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Apps.AppInstallService.ArcAppInstaller.InstallResult.",
                    base::ToString(surface)}),
      result);
}
}  // namespace

ArcAppInstaller::ArcAppInstaller(Profile* profile) : profile_(profile) {
  if (auto* arc_service_manager = arc::ArcServiceManager::Get()) {
    arc_service_manager->arc_bridge_service()->app()->AddObserver(this);
  }
}

ArcAppInstaller::~ArcAppInstaller() {
  if (auto* arc_service_manager = arc::ArcServiceManager::Get()) {
    arc_service_manager->arc_bridge_service()->app()->RemoveObserver(this);
  }
}

void ArcAppInstaller::InstallApp(AppInstallSurface surface,
                                 AppInstallData data,
                                 ArcAppInstalledCallback callback) {
  CHECK(absl::holds_alternative<AndroidAppInstallData>(data.app_type_data));

  // Installation is only allowed from specific surfaces, while we build a
  // general-purpose installation method.
  if (surface != AppInstallSurface::kAppPreloadServiceOem &&
      surface != AppInstallSurface::kAppPreloadServiceDefault &&
      surface != AppInstallSurface::kOobeAppRecommendations) {
    std::move(callback).Run(false);
    return;
  }

  pending_android_installs_.emplace_back(surface, data.package_id.identifier(),
                                         std::move(callback));
  InstallPendingAndroidApps();
}

ArcAppInstaller::PendingAndroidInstall::PendingAndroidInstall(
    AppInstallSurface surface,
    std::string package_name,
    ArcAppInstalledCallback callback)
    : surface(surface),
      package_name(std::move(package_name)),
      callback(std::move(callback)) {}
ArcAppInstaller::PendingAndroidInstall::PendingAndroidInstall(
    PendingAndroidInstall&& other) noexcept = default;
ArcAppInstaller::PendingAndroidInstall&
ArcAppInstaller::PendingAndroidInstall::operator=(
    ArcAppInstaller::PendingAndroidInstall&& other) noexcept = default;
ArcAppInstaller::PendingAndroidInstall::~PendingAndroidInstall() = default;

void ArcAppInstaller::OnConnectionReady() {
  InstallPendingAndroidApps();
}

void ArcAppInstaller::InstallPendingAndroidApps() {
  if (pending_android_installs_.empty()) {
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(ERROR) << "Could not get arc mgr";
    return;
  }

  arc::mojom::AppInstance* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->app(),
      StartFastAppReinstallFlow);
  if (!app_instance) {
    LOG(ERROR)
        << "Could not get AppInstance, connected="
        << arc_service_manager->arc_bridge_service()->app()->IsConnected();
    return;
  }

  // Get all packages into a single vector to install.
  std::vector<std::string> packages;
  for (auto& install : pending_android_installs_) {
    packages.push_back(install.package_name);
  }
  app_instance->StartFastAppReinstallFlow(packages);

  // Invoke callbacks and UMAs.
  // TODO(b/336694386): Track outcome rather than assuming success.
  for (auto& install : pending_android_installs_) {
    RecordInstallResultMetric(install.surface, ArcAppInstallResult::kSuccess);
    std::move(install.callback).Run(true);
  }
  pending_android_installs_.clear();
}

}  // namespace apps
