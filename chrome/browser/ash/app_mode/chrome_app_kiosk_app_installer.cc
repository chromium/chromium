// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/chrome_app_kiosk_app_installer.h"

#include "base/syslog_logging.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

namespace ash {

ChromeAppKioskAppInstaller::ChromeAppKioskAppInstaller(
    Profile* profile,
    const std::string& app_id,
    KioskAppLauncher::Delegate* delegate)
    : profile_(profile), app_id_(app_id), delegate_(delegate) {}

ChromeAppKioskAppInstaller::~ChromeAppKioskAppInstaller() {}

void ChromeAppKioskAppInstaller::BeginInstall(InstallCallback callback) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "BeginInstall";

  on_ready_callback_ = std::move(callback);

  extensions::file_util::SetUseSafeInstallation(true);
  KioskAppManager::Get()->UpdatePrimaryAppLoaderPrefs(app_id_);
  if (IsAppInstallPending(app_id_)) {
    delegate_->OnAppInstalling();
    ObserveActiveInstallations();
    return;
  }

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  if (!primary_app) {
    // The extension is skipped for installation due to some error.
    ReportInstallFailure(KioskAppLaunchError::Error::kUnableToInstall);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    // The installed primary app is not kiosk enabled.
    ReportInstallFailure(KioskAppLaunchError::Error::kNotKioskEnabled);
    return;
  }

  // Install secondary apps.
  MaybeInstallSecondaryApps();
}

void ChromeAppKioskAppInstaller::MaybeInstallSecondaryApps() {
  if (install_complete_)
    return;

  if (!AreSecondaryAppsInstalled() && !delegate_->IsNetworkReady()) {
    RetryWhenNetworkIsAvailable(
        base::BindOnce(&ChromeAppKioskAppInstaller::MaybeInstallSecondaryApps,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  secondary_apps_installed_ = true;
  extensions::KioskModeInfo* info =
      extensions::KioskModeInfo::Get(GetPrimaryAppExtension());

  std::vector<std::string> secondary_app_ids;
  for (const auto& app : info->secondary_apps)
    secondary_app_ids.push_back(app.id);

  KioskAppManager::Get()->UpdateSecondaryAppsLoaderPrefs(secondary_app_ids);
  if (IsAnySecondaryAppPending()) {
    delegate_->OnAppInstalling();
    ObserveActiveInstallations();
    return;
  }

  if (AreSecondaryAppsInstalled()) {
    // Check extension update before launching the primary kiosk app.
    MaybeCheckExtensionUpdate();
  } else {
    ReportInstallFailure(KioskAppLaunchError::Error::kUnableToInstall);
  }
}

void ChromeAppKioskAppInstaller::MaybeCheckExtensionUpdate() {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "MaybeCheckExtensionUpdate";
  if (!delegate_->IsNetworkReady()) {
    FinalizeAppInstall();
    return;
  }

  // Enforce an immediate version update check for all extensions before
  // launching the primary app. After the chromeos is updated, the shared
  // module(e.g. ARC runtime) may need to be updated to a newer version
  // compatible with the new chromeos. See crbug.com/555083.
  update_checker_ = std::make_unique<StartupAppLauncherUpdateChecker>(profile_);
  if (!update_checker_->Run(base::BindOnce(
          &ChromeAppKioskAppInstaller::OnExtensionUpdateCheckFinished,
          weak_ptr_factory_.GetWeakPtr()))) {
    update_checker_.reset();
    FinalizeAppInstall();
    return;
  }

  SYSLOG(INFO) << "Extension update check run.";
}

void ChromeAppKioskAppInstaller::OnExtensionUpdateCheckFinished(
    bool update_found) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "OnExtensionUpdateCheckFinished";
  update_checker_.reset();
  if (update_found) {
    SYSLOG(INFO) << "Start to reload extension with id " << app_id_;

    // Reload the primary app to make sure any reference to the previous version
    // of the shared module, extension, etc will be cleaned up and the new
    // version will be loaded.
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->ReloadExtension(app_id_);

    SYSLOG(INFO) << "Finish to reload extension with id " << app_id_;
  }

  FinalizeAppInstall();
}

void ChromeAppKioskAppInstaller::FinalizeAppInstall() {
  DCHECK(!install_complete_);

  std::move(on_ready_callback_).Run(KioskAppLaunchError::Error::kNone);
}

void ChromeAppKioskAppInstaller::OnFinishCrxInstall(
    const std::string& extension_id,
    bool success) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "OnFinishCrxInstall, id=" << extension_id
               << ", success=" << success;

  if (DidPrimaryOrSecondaryAppFailedToInstall(success, extension_id)) {
    install_observation_.Reset();
    ReportInstallFailure(KioskAppLaunchError::Error::kUnableToInstall);
    return;
  }

  // Wait for pending updates or dependent extensions to download.
  if (extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->pending_extension_manager()
          ->HasPendingExtensions()) {
    return;
  }

  install_observation_.Reset();
  if (delegate_->IsShowingNetworkConfigScreen()) {
    SYSLOG(WARNING) << "Showing network config screen";
    return;
  }

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  if (!primary_app) {
    ReportInstallFailure(KioskAppLaunchError::Error::kUnableToInstall);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    ReportInstallFailure(KioskAppLaunchError::Error::kNotKioskEnabled);
    return;
  }

  if (!secondary_apps_installed_)
    MaybeInstallSecondaryApps();
  else
    MaybeCheckExtensionUpdate();
}

void ChromeAppKioskAppInstaller::ReportInstallFailure(
    KioskAppLaunchError::Error error) {
  SYSLOG(ERROR) << "App launch failed, error: " << static_cast<int>(error);
  DCHECK_NE(KioskAppLaunchError::Error::kNone, error);

  std::move(on_ready_callback_).Run(error);
}

void ChromeAppKioskAppInstaller::RetryWhenNetworkIsAvailable(
    base::OnceClosure callback) {
  DelayNetworkCall(base::Milliseconds(kDefaultNetworkRetryDelayMS),
                   std::move(callback));
}

void ChromeAppKioskAppInstaller::ObserveActiveInstallations() {
  install_observation_.Observe(
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_));
}

const extensions::Extension*
ChromeAppKioskAppInstaller::GetPrimaryAppExtension() const {
  return extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
      app_id_);
}

bool ChromeAppKioskAppInstaller::AreSecondaryAppsInstalled() const {
  const extensions::Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(extension);
  for (const auto& app : info->secondary_apps) {
    if (!extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
            app.id)) {
      return false;
    }
  }
  return true;
}

bool ChromeAppKioskAppInstaller::IsAppInstallPending(
    const std::string& id) const {
  return extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->pending_extension_manager()
      ->IsIdPending(id);
}

bool ChromeAppKioskAppInstaller::IsAnySecondaryAppPending() const {
  const extensions::Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(extension);
  for (const auto& app : info->secondary_apps) {
    if (IsAppInstallPending(app.id)) {
      return true;
    }
  }
  return false;
}

bool ChromeAppKioskAppInstaller::DidPrimaryOrSecondaryAppFailedToInstall(
    bool success,
    const std::string& id) const {
  if (success)
    return false;

  if (id == app_id_) {
    SYSLOG(ERROR) << "Failed to install crx file of the primary app id=" << id;
    return true;
  }

  const extensions::Extension* extension = GetPrimaryAppExtension();
  if (!extension)
    return false;

  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(extension);
  for (const auto& app : info->secondary_apps) {
    if (app.id == id) {
      SYSLOG(ERROR) << "Failed to install a secondary app id=" << id;
      return true;
    }
  }

  SYSLOG(WARNING) << "Failed to install crx file for an app id=" << id;
  return false;
}

}  // namespace ash