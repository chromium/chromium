// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"

#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

namespace chromeos {

namespace {

const char kChromeKioskExtensionUpdateErrorHistogram[] =
    "Kiosk.ChromeApp.ExtensionUpdateError";
const char kChromeKioskExtensionHasUpdateDurationHistogram[] =
    "Kiosk.ChromeApp.ExtensionUpdateDuration.HasUpdate";
const char kChromeKioskExtensionNoUpdateDurationHistogram[] =
    "Kiosk.ChromeApp.ExtensionUpdateDuration.NoUpdate";

}  // namespace

ChromeKioskAppInstaller::ChromeKioskAppInstaller(
    Profile* profile,
    const AppInstallParams& install_data)
    : profile_(profile), primary_app_install_data_(install_data) {}

ChromeKioskAppInstaller::~ChromeKioskAppInstaller() = default;

void ChromeKioskAppInstaller::BeginInstall(InstallCallback callback) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "BeginInstall";

  on_ready_callback_ = std::move(callback);

  extensions::file_util::SetUseSafeInstallation(true);

  if (primary_app_install_data_.crx_file_location.empty() &&
      !GetPrimaryAppExtension()) {
    ReportInstallFailure(InstallResult::kPrimaryAppNotCached);
    return;
  }

  ChromeKioskExternalLoaderBroker::Get()->TriggerPrimaryAppInstall(
      primary_app_install_data_);
  if (IsAppInstallPending(primary_app_install_data_.id)) {
    ObserveActiveInstallations();
    return;
  }

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  if (!primary_app) {
    // The extension is skipped for installation due to some error.
    ReportInstallFailure(InstallResult::kPrimaryAppInstallFailed);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    // The installed primary app is not kiosk enabled.
    ReportInstallFailure(InstallResult::kPrimaryAppNotKioskEnabled);
    return;
  }

  // Install secondary apps.
  MaybeInstallSecondaryApps();
}

void ChromeKioskAppInstaller::MaybeInstallSecondaryApps() {
  if (install_complete_) {
    return;
  }

  secondary_apps_installing_ = true;
  extensions::KioskModeInfo* info =
      extensions::KioskModeInfo::Get(GetPrimaryAppExtension());

  std::vector<std::string> secondary_app_ids;
  for (const auto& app : info->secondary_apps) {
    secondary_app_ids.push_back(app.id);
  }

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
    ReportInstallFailure(InstallResult::kSecondaryAppInstallFailed);
  }
}

void ChromeKioskAppInstaller::MaybeCheckExtensionUpdate() {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "MaybeCheckExtensionUpdate";

  // Record update start time to calculate time consumed by update check. When
  // `OnExtensionUpdateCheckFinished` is called the update is already finished
  // because `extensions::ExtensionUpdater::CheckParams::install_immediately` is
  // set to true.
  extension_update_start_time_ = base::Time::Now();

  // Observe installation failures.
  install_stage_observation_.Observe(
      extensions::InstallStageTracker::Get(profile_));

  // Enforce an immediate version update check for all extensions before
  // launching the primary app. After the chromeos is updated, the shared
  // module(e.g. ARC runtime) may need to be updated to a newer version
  // compatible with the new chromeos. See crbug.com/555083.
  update_checker_ = std::make_unique<StartupAppLauncherUpdateChecker>(profile_);
  if (!update_checker_->Run(base::BindOnce(
          &ChromeKioskAppInstaller::OnExtensionUpdateCheckFinished,
          weak_ptr_factory_.GetWeakPtr()))) {
    update_checker_.reset();
    install_stage_observation_.Reset();
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
  install_stage_observation_.Reset();
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

  base::UmaHistogramMediumTimes(
      update_found ? kChromeKioskExtensionHasUpdateDurationHistogram
                   : kChromeKioskExtensionNoUpdateDurationHistogram,
      base::Time::Now() - extension_update_start_time_);

  FinalizeAppInstall();
}

void ChromeKioskAppInstaller::FinalizeAppInstall() {
  DCHECK(!install_complete_);

  install_complete_ = true;

  ReportInstallSuccess();
}

void ChromeKioskAppInstaller::OnFinishCrxInstall(
    content::BrowserContext* context,
    const extensions::CrxInstaller& installer,
    const std::string& extension_id,
    bool success) {
  DCHECK(!install_complete_);

  SYSLOG(INFO) << "OnFinishCrxInstall, id=" << extension_id
               << ", success=" << success;

  if (DidPrimaryOrSecondaryAppFailedToInstall(success, extension_id)) {
    install_observation_.Reset();
    ReportInstallFailure((extension_id == primary_app_install_data_.id)
                             ? InstallResult::kPrimaryAppInstallFailed
                             : InstallResult::kSecondaryAppInstallFailed);
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

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  if (!primary_app) {
    ReportInstallFailure(InstallResult::kPrimaryAppInstallFailed);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    ReportInstallFailure(InstallResult::kPrimaryAppNotKioskEnabled);
    return;
  }

  if (!secondary_apps_installing_) {
    MaybeInstallSecondaryApps();
  } else {
    MaybeCheckExtensionUpdate();
  }
}

void ChromeKioskAppInstaller::OnExtensionInstallationFailed(
    const extensions::ExtensionId& id,
    extensions::InstallStageTracker::FailureReason reason) {
  base::UmaHistogramEnumeration(kChromeKioskExtensionUpdateErrorHistogram,
                                reason);
}

void ChromeKioskAppInstaller::ReportInstallSuccess() {
  DCHECK(install_complete_);
  SYSLOG(INFO) << "Kiosk app is ready to launch.";

  std::move(on_ready_callback_)
      .Run(ChromeKioskAppInstaller::InstallResult::kSuccess);
}

void ChromeKioskAppInstaller::ReportInstallFailure(
    ChromeKioskAppInstaller::InstallResult error) {
  SYSLOG(ERROR) << "App install failed, error: " << static_cast<int>(error);
  DCHECK_NE(ChromeKioskAppInstaller::InstallResult::kSuccess, error);

  std::move(on_ready_callback_).Run(error);
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
  if (success) {
    return false;
  }

  if (id == primary_app_install_data_.id) {
    SYSLOG(ERROR) << "Failed to install crx file of the primary app id=" << id;
    return true;
  }

  const extensions::Extension* extension = GetPrimaryAppExtension();
  if (!extension) {
    return false;
  }

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

}  // namespace chromeos
