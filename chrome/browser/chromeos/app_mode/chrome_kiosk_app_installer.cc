// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"

#include "base/syslog_logging.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"

namespace ash {

ChromeKioskAppInstaller::AppInstallData::AppInstallData() = default;
ChromeKioskAppInstaller::AppInstallData::AppInstallData(
    const AppInstallData& other) = default;
ChromeKioskAppInstaller::AppInstallData&
ChromeKioskAppInstaller::AppInstallData::operator=(
    const AppInstallData& other) = default;
ChromeKioskAppInstaller::AppInstallData::~AppInstallData() = default;

ChromeKioskAppInstaller::ChromeKioskAppInstaller(
    Profile* profile,
    const AppInstallData& install_data,
    KioskAppLauncher::Delegate* delegate,
    bool finalize_only)
    : profile_(profile),
      primary_app_install_data_(install_data),
      delegate_(delegate),
      finalize_only_(finalize_only) {}

ChromeKioskAppInstaller::~ChromeKioskAppInstaller() {}

void ChromeKioskAppInstaller::BeginInstall(InstallCallback callback) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "BeginInstall";

  on_ready_callback_ = std::move(callback);

  if (finalize_only_) {
    FinalizeAppInstall();
    return;
  }

  extensions::file_util::SetUseSafeInstallation(true);
  ChromeKioskExternalLoaderBroker::Get()->TriggerPrimaryAppInstall(
      primary_app_install_data_);
  if (IsAppInstallPending(primary_app_install_data_.id)) {
    ObserveActiveInstallations();
    return;
  }

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  if (!primary_app) {
    // The extension is skipped for installation due to some error.
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kUnableToInstall);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    // The installed primary app is not kiosk enabled.
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kNotKioskEnabled);
    return;
  }

  // Install secondary apps.
  MaybeInstallSecondaryApps();
}

void ChromeKioskAppInstaller::MaybeInstallSecondaryApps() {
  if (install_complete_)
    return;

  if (!AreSecondaryAppsInstalled() && !delegate_->IsNetworkReady()) {
    RetryWhenNetworkIsAvailable(
        base::BindOnce(&ChromeKioskAppInstaller::MaybeInstallSecondaryApps,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  secondary_apps_installed_ = true;
  extensions::KioskModeInfo* info =
      extensions::KioskModeInfo::Get(GetPrimaryAppExtension());

  std::vector<std::string> secondary_app_ids;
  for (const auto& app : info->secondary_apps)
    secondary_app_ids.push_back(app.id);

  ChromeKioskExternalLoaderBroker::Get()->TriggerSecondaryAppInstall(
      secondary_app_ids);
  if (IsAnySecondaryAppPending()) {
    ObserveActiveInstallations();
    return;
  }

  if (AreSecondaryAppsInstalled()) {
    // Check extension update before launching the primary kiosk app.
    MaybeCheckExtensionUpdate();
  } else {
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kUnableToInstall);
  }
}

void ChromeKioskAppInstaller::MaybeCheckExtensionUpdate() {
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
          &ChromeKioskAppInstaller::OnExtensionUpdateCheckFinished,
          weak_ptr_factory_.GetWeakPtr()))) {
    update_checker_.reset();
    FinalizeAppInstall();
    return;
  }

  SYSLOG(INFO) << "Extension update check run.";
}

void ChromeKioskAppInstaller::OnExtensionUpdateCheckFinished(
    bool update_found) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "OnExtensionUpdateCheckFinished";
  update_checker_.reset();
  if (update_found) {
    SYSLOG(INFO) << "Start to reload extension with id "
                 << primary_app_install_data_.id;

    // Reload the primary app to make sure any reference to the previous version
    // of the shared module, extension, etc will be cleaned up and the new
    // version will be loaded.
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->ReloadExtension(primary_app_install_data_.id);

    SYSLOG(INFO) << "Finish to reload extension with id "
                 << primary_app_install_data_.id;
  }

  FinalizeAppInstall();
}

void ChromeKioskAppInstaller::FinalizeAppInstall() {
  DCHECK(!install_complete_);

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  // Verify that required apps are installed. While the apps should be
  // present at this point, crash recovery flow skips app installation steps -
  // this means that the kiosk app might not yet be downloaded. If that is
  // the case, bail out from the app launch.
  if (!primary_app || !AreSecondaryAppsInstalled()) {
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kUnableToLaunch);
    return;
  }

  const bool offline_enabled =
      extensions::OfflineEnabledInfo::IsOfflineEnabled(primary_app);
  // If the app is not offline enabled, make sure the network is ready before
  // launching.
  if (!offline_enabled && !delegate_->IsNetworkReady()) {
    ReportInstallFailure(InstallResult::kNetworkMissing);
    return;
  }

  install_complete_ = true;

  SetSecondaryAppsEnabledState(primary_app);
  MaybeUpdateAppData();

  ReportInstallSuccess();
}

void ChromeKioskAppInstaller::MaybeUpdateAppData() {
  // Skip copying meta data from the current installed primary app when
  // there is a pending update.
  if (PrimaryAppHasPendingUpdate())
    return;

  KioskAppManager::Get()->ClearAppData(primary_app_install_data_.id);
  KioskAppManager::Get()->UpdateAppDataFromProfile(primary_app_install_data_.id,
                                                   profile_, NULL);
}

void ChromeKioskAppInstaller::OnFinishCrxInstall(
    const std::string& extension_id,
    bool success) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "OnFinishCrxInstall, id=" << extension_id
               << ", success=" << success;

  if (DidPrimaryOrSecondaryAppFailedToInstall(success, extension_id)) {
    install_observation_.Reset();
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kUnableToInstall);
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
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kUnableToInstall);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    ReportInstallFailure(
        ChromeKioskAppInstaller::InstallResult::kNotKioskEnabled);
    return;
  }

  if (!secondary_apps_installed_)
    MaybeInstallSecondaryApps();
  else
    MaybeCheckExtensionUpdate();
}

void ChromeKioskAppInstaller::ReportInstallSuccess() {
  DCHECK(install_complete_);
  SYSLOG(INFO) << "Kiosk app is ready to launch.";

  std::move(on_ready_callback_)
      .Run(ChromeKioskAppInstaller::InstallResult::kSuccess);
}

void ChromeKioskAppInstaller::ReportInstallFailure(
    ChromeKioskAppInstaller::InstallResult error) {
  SYSLOG(ERROR) << "App launch failed, error: " << static_cast<int>(error);
  DCHECK_NE(ChromeKioskAppInstaller::InstallResult::kSuccess, error);

  std::move(on_ready_callback_).Run(error);
}

void ChromeKioskAppInstaller::RetryWhenNetworkIsAvailable(
    base::OnceClosure callback) {
  DelayNetworkCall(base::Milliseconds(kDefaultNetworkRetryDelayMS),
                   std::move(callback));
}

void ChromeKioskAppInstaller::ObserveActiveInstallations() {
  install_observation_.Observe(
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_));
}

const extensions::Extension* ChromeKioskAppInstaller::GetPrimaryAppExtension()
    const {
  return extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
      primary_app_install_data_.id);
}

bool ChromeKioskAppInstaller::AreSecondaryAppsInstalled() const {
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

bool ChromeKioskAppInstaller::IsAppInstallPending(const std::string& id) const {
  return extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->pending_extension_manager()
      ->IsIdPending(id);
}

bool ChromeKioskAppInstaller::IsAnySecondaryAppPending() const {
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

bool ChromeKioskAppInstaller::PrimaryAppHasPendingUpdate() const {
  return extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->GetPendingExtensionUpdate(primary_app_install_data_.id);
}

bool ChromeKioskAppInstaller::DidPrimaryOrSecondaryAppFailedToInstall(
    bool success,
    const std::string& id) const {
  if (success)
    return false;

  if (id == primary_app_install_data_.id) {
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

void ChromeKioskAppInstaller::SetSecondaryAppsEnabledState(
    const extensions::Extension* primary_app) {
  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(primary_app);
  for (const auto& app_info : info->secondary_apps) {
    // If the enabled on launch is not specified in the manifest, the apps
    // enabled state should be kept as is.
    if (!app_info.enabled_on_launch.has_value())
      continue;

    SetAppEnabledState(app_info.id, app_info.enabled_on_launch.value());
  }
}

void ChromeKioskAppInstaller::SetAppEnabledState(
    const extensions::ExtensionId& id,
    bool new_enabled_state) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);

  // If the app is already enabled, and we want it to be enabled, nothing to do.
  if (service->IsExtensionEnabled(id) && new_enabled_state) {
    return;
  }

  if (new_enabled_state) {
    // Remove USER_ACTION disable reason - if no other disabled reasons are
    // present, enable the app.
    prefs->RemoveDisableReason(id,
                               extensions::disable_reason::DISABLE_USER_ACTION);
    if (prefs->GetDisableReasons(id) ==
        extensions::disable_reason::DISABLE_NONE) {
      service->EnableExtension(id);
    }
  } else {
    service->DisableExtension(id,
                              extensions::disable_reason::DISABLE_USER_ACTION);
  }
}

}  // namespace ash
