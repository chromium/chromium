// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/bruschetta_apps.h"

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/bruschetta/bruschetta_features.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

namespace {

void AddSpinner(const std::string& app_id) {
  if (auto* chrome_controller = ChromeShelfController::instance()) {
    chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
        app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  }
}

void RemoveSpinner(const std::string& app_id) {
  if (auto* chrome_controller = ChromeShelfController::instance()) {
    chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
  }
}

void LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id) {
  // TODO(b/265601951): Handle window permissions. Crostini uses
  // AppServiceAppWindowCrostiniTracker::OnAppLaunchRequested for this.
  // TODO(b/245412929): Share paths to files.
  const guest_os::GuestId container_id(registration.VmType(),
                                       registration.VmName(),
                                       registration.ContainerName());

  std::vector<std::string> files;
  // TODO(b/265601951): Factor this out of CrostiniManager.
  crostini::CrostiniManager::GetForProfile(profile)->LaunchContainerApplication(
      container_id, registration.DesktopFileId(), files,
      registration.IsScaled(),
      base::BindOnce(
          [](const std::string& app_id, bool success,
             const std::string& failure_reason) {
            if (!success) {
              LOG(ERROR) << "Failed to launch Bruschetta app " << app_id << ": "
                         << failure_reason;
            }
            RemoveSpinner(app_id);
          },
          app_id));
}

}  // namespace

BruschettaApps::BruschettaApps(AppServiceProxy* proxy) : GuestOSApps(proxy) {}

bool BruschettaApps::CouldBeAllowed() const {
  return bruschetta::BruschettaFeatures::Get()->IsEnabled();
}

apps::AppType BruschettaApps::AppType() const {
  return AppType::kBruschetta;
}

guest_os::VmType BruschettaApps::VmType() const {
  return guest_os::VmType::BRUSCHETTA;
}

void BruschettaApps::LoadIcon(const std::string& app_id,
                              const IconKey& icon_key,
                              IconType icon_type,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              apps::LoadIconCallback callback) {
  // TODO(b/247636749): Consider creating IDR_LOGO_BRUSCHETTA_DEFAULT
  // to replace IconKey::kInvalidResourceId.
  registry()->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                       allow_placeholder_icon, IconKey::kInvalidResourceId,
                       std::move(callback));
}

void BruschettaApps::Launch(const std::string& app_id,
                            int32_t event_flags,
                            LaunchSource launch_source,
                            WindowInfoPtr window_info) {
  const int64_t display_id =
      window_info ? window_info->display_id : display::kInvalidDisplayId;
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry()->GetRegistration(app_id);
  if (!registration) {
    // TODO(b/247638226): RecordAppLaunchHistogram(kUnknown) to collect usage
    // stats for failed launches.
    LOG(ERROR) << "BruschettaApps::Launch called with an unknown app_id: "
               << app_id;
    return;
  }
  // TODO(b/247638226): RecordAppLaunchHistogram(kRegisteredApp) to collect
  // usage stats for successful launches.

  // Update the last launched time.
  registry()->AppLaunched(app_id);

  // Start the bruschetta VM if necessary.
  const std::string& vm_name = registration->VmName();
  auto launcher =
      bruschetta::BruschettaService::GetForProfile(profile())->GetLauncher(
          vm_name);
  if (!launcher) {
    LOG(ERROR) << "Unknown Bruschetta VM name: " << vm_name;
    return;
  }
  AddSpinner(app_id);
  launcher->EnsureRunning(base::BindOnce(
      [](Profile* profile, const std::string& app_id,
         guest_os::GuestOsRegistryService::Registration registration,
         int64_t display_id, const std::string& vm_name,
         bruschetta::BruschettaResult result) {
        if (result != bruschetta::BruschettaResult::kSuccess) {
          LOG(ERROR) << "Failed to start Bruschetta VM " << vm_name << ": "
                     << bruschetta::BruschettaResultString(result);
          RemoveSpinner(app_id);
          return;
        }
        LaunchApplication(profile, app_id, std::move(registration), display_id);
      },
      profile(), app_id, std::move(registration.value()), display_id, vm_name));
}

void BruschettaApps::LaunchAppWithParams(AppLaunchParams&& params,
                                         LaunchCallback callback) {
  // TODO(b/265601951): Implement this.
}

void BruschettaApps::CreateAppOverrides(
    const guest_os::GuestOsRegistryService::Registration& registration,
    App* app) {
  // TODO(b/247638042): Implement IsUninstallable and use it here.
  // TODO(b/245412929): Implement intent filter and use it here.
}

}  // namespace apps
