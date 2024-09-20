// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_uninstaller.h"

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"

namespace borealis {

BorealisAppUninstaller::BorealisAppUninstaller(Profile* profile)
    : profile_(profile) {}

void BorealisAppUninstaller::Uninstall(std::string app_id,
                                       OnUninstalledCallback callback) {
  if (app_id == kInstallerAppId || app_id == kClientAppId) {
    BorealisServiceFactory::GetForProfile(profile_)->Installer().Uninstall(
        base::BindOnce(
            [](OnUninstalledCallback callback, BorealisUninstallResult result) {
              if (result != BorealisUninstallResult::kSuccess) {
                LOG(ERROR) << "Failed to uninstall borealis";
                std::move(callback).Run(UninstallResult::kError);
                return;
              }
              std::move(callback).Run(UninstallResult::kSuccess);
            },
            std::move(callback)));
    return;
  }

  std::optional<guest_os::GuestOsRegistryService::Registration> registration =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
          ->GetRegistration(app_id);
  if (!registration.has_value()) {
    LOG(ERROR) << "Tried to uninstall an application that does not exist in "
                  "the registry";
    std::move(callback).Run(UninstallResult::kError);
    return;
  }
  std::optional<int> uninstall_app_id = ParseSteamGameId(registration->Exec());
  if (!uninstall_app_id.has_value()) {
    LOG(ERROR) << "Couldn't retrieve the borealis app id from the exec "
                  "information provided";
    std::move(callback).Run(UninstallResult::kError);
    return;
  }
  std::optional<guest_os::GuestOsRegistryService::Registration> main_app =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
          ->GetRegistration(kClientAppId);
  if (!main_app.has_value()) {
    LOG(ERROR) << "Failed to retrieve a registration for the Borealis main app";
    std::move(callback).Run(UninstallResult::kError);
    return;
  }
  std::string uninstall_string =
      "steam://uninstall/" + base::NumberToString(*uninstall_app_id);
  borealis::BorealisServiceFactory::GetForProfile(profile_)
      ->AppLauncher()
      .Launch(kClientAppId, {uninstall_string},
              borealis::BorealisLaunchSource::kAppUninstaller,
              base::BindOnce(
                  [](OnUninstalledCallback callback,
                     BorealisAppLauncher::LaunchResult result) {
                    if (result != BorealisAppLauncher::LaunchResult::kSuccess) {
                      LOG(ERROR)
                          << "Failed to uninstall a borealis application";
                      std::move(callback).Run(UninstallResult::kError);
                      return;
                    }
                    std::move(callback).Run(UninstallResult::kSuccess);
                  },
                  std::move(callback)));
}

}  // namespace borealis
