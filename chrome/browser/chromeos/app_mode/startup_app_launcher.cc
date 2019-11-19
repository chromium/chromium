// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_diagnosis_runner.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/id_util.h"
#include "components/session_manager/core/session_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "net/base/load_flags.h"

using extensions::Extension;

namespace chromeos {

namespace {

const int kMaxLaunchAttempt = 5;

}  // namespace

StartupAppLauncher::StartupAppLauncher(Profile* profile,
                                       const std::string& app_id,
                                       bool diagnostic_mode,
                                       StartupAppLauncher::Delegate* delegate)
    : profile_(profile),
      app_id_(app_id),
      diagnostic_mode_(diagnostic_mode),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(crx_file::id_util::IdIsValid(app_id_));
  kiosk_app_manager_observer_.Add(KioskAppManager::Get());
}

StartupAppLauncher::~StartupAppLauncher() = default;

void StartupAppLauncher::Initialize() {
  MaybeInitializeNetwork();
}

void StartupAppLauncher::ContinueWithNetworkReady() {
  SYSLOG(INFO) << "ContinueWithNetworkReady"
               << ", network_ready_handled_=" << network_ready_handled_
               << ", ready_to_launch_=" << ready_to_launch_;

  if (ready_to_launch_ || network_ready_handled_)
    return;

  network_ready_handled_ = true;

  if (delegate_->ShouldSkipAppInstallation()) {
    MaybeLaunchApp();
    return;
  }

  // The network might not be ready when KioskAppManager tries to update
  // external cache initially. Update the external cache now that the network
  // is ready for sure.
  wait_for_crx_update_ = true;
  KioskAppManager::Get()->UpdateExternalCache();
}

void StartupAppLauncher::RestartLauncher() {
  // Do not allow restarts after the launcher finishes kiosk apps installation -
  // notify the delegate that kiosk app is ready to launch, in case the launch
  // was delayed, for example by network config dialog.
  if (ready_to_launch_) {
    delegate_->OnReadyToLaunch();
    return;
  }

  // If the installer is still running in the background, we don't need to
  // restart the launch process. We will just wait until it completes and
  // launches the kiosk app.
  if (extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->pending_extension_manager()
          ->IsIdPending(app_id_)) {
    SYSLOG(WARNING) << "Installer still running";
    return;
  }

  MaybeInitializeNetwork();
}

void StartupAppLauncher::MaybeInitializeNetwork() {
  DCHECK(!ready_to_launch_);

  network_ready_handled_ = false;

  const Extension* extension = GetPrimaryAppExtension();
  bool crx_cached = KioskAppManager::Get()->HasCachedCrx(app_id_);
  const bool requires_network =
      (!extension && !crx_cached) ||
      (extension &&
       !extensions::OfflineEnabledInfo::IsOfflineEnabled(extension));

  SYSLOG(INFO) << "MaybeInitializeNetwork"
               << ", requires_network=" << requires_network
               << ", network_ready=" << delegate_->IsNetworkReady();

  if (requires_network) {
    delegate_->InitializeNetwork();
    return;
  }

  if (delegate_->ShouldSkipAppInstallation()) {
    MaybeLaunchApp();
    return;
  }

  // Update the offline enabled crx cache if the network is ready;
  // or just install the app.
  if (delegate_->IsNetworkReady())
    ContinueWithNetworkReady();
  else
    BeginInstall();
}

void StartupAppLauncher::SetSecondaryAppsEnabledState(
    const extensions::Extension* primary_app) {
  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(primary_app);
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  for (const auto& app_info : info->secondary_apps) {
    // If the enabled on launch is not specified in the manifest, the apps
    // enabled state should be kept as is.
    if (!app_info.enabled_on_launch.has_value())
      continue;

    // If the app is already enabled, and should not be disabled, there is
    // nothing to do for the app.
    if (app_info.enabled_on_launch.value() &&
        service->IsExtensionEnabled(app_info.id)) {
      continue;
    }

    if (!app_info.enabled_on_launch.value()) {
      service->DisableExtension(
          app_info.id, extensions::disable_reason::DISABLE_USER_ACTION);
    } else {
      // Remove USER_ACTION disable reason - if the app was disabled only due to
      // user action, enable it.
      if (prefs->GetDisableReasons(app_info.id) ==
          extensions::disable_reason::DISABLE_USER_ACTION) {
        service->EnableExtension(app_info.id);
      } else {
        prefs->RemoveDisableReason(
            app_info.id, extensions::disable_reason::DISABLE_USER_ACTION);
      }
    }
  }
}

void StartupAppLauncher::MaybeLaunchApp() {
  DCHECK(!ready_to_launch_);

  SYSLOG(INFO) << "MaybeLaunchApp";
  const Extension* extension = GetPrimaryAppExtension();
  // Verify that requred apps are installed. While the apps should be
  // present at this point, crash recovery flow skips app installation steps -
  // this means that the kiosk app might not yet be downloaded. If that is
  // the case, bail out from the app launch.
  if (!extension || !AreSecondaryAppsInstalled()) {
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_LAUNCH);
    return;
  }

  const bool offline_enabled =
      extensions::OfflineEnabledInfo::IsOfflineEnabled(extension);
  // If the app is not offline enabled, make sure the network is ready before
  // launching.
  if (offline_enabled || delegate_->IsNetworkReady()) {
    ready_to_launch_ = true;
    // Updates to cached primary app crx will be ignored after this point, so
    // there is no need to observe the kiosk app manager any longer.
    kiosk_app_manager_observer_.RemoveAll();

    SetSecondaryAppsEnabledState(extension);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&StartupAppLauncher::OnReadyToLaunch,
                                  weak_ptr_factory_.GetWeakPtr()));
  } else {
    ++launch_attempt_;
    if (launch_attempt_ < kMaxLaunchAttempt) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&StartupAppLauncher::MaybeInitializeNetwork,
                                    weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_LAUNCH);
  }
}

void StartupAppLauncher::MaybeCheckExtensionUpdate() {
  DCHECK(!ready_to_launch_);

  SYSLOG(INFO) << "MaybeCheckExtensionUpdate";
  if (!delegate_->IsNetworkReady()) {
    MaybeLaunchApp();
    return;
  }

  // Enforce an immediate version update check for all extensions before
  // launching the primary app. After the chromeos is updated, the shared
  // module(e.g. ARC runtime) may need to be updated to a newer version
  // compatible with the new chromeos. See crbug.com/555083.
  update_checker_ = std::make_unique<StartupAppLauncherUpdateChecker>(profile_);
  if (!update_checker_->Run(
          base::BindOnce(&StartupAppLauncher::OnExtensionUpdateCheckFinished,
                         weak_ptr_factory_.GetWeakPtr()))) {
    update_checker_.reset();
    MaybeLaunchApp();
    return;
  }

  SYSLOG(INFO) << "Extension update check run.";
}

void StartupAppLauncher::OnExtensionUpdateCheckFinished(bool update_found) {
  DCHECK(!ready_to_launch_);

  SYSLOG(INFO) << "OnExtensionUpdateCheckFinished";
  update_checker_.reset();
  if (update_found) {
    // Reload the primary app to make sure any reference to the previous version
    // of the shared module, extension, etc will be cleaned up andthe new
    // version will be loaded.
    extensions::ExtensionSystem::Get(profile_)
            ->extension_service()
            ->ReloadExtension(app_id_);
  }

  MaybeLaunchApp();
}

void StartupAppLauncher::OnFinishCrxInstall(const std::string& extension_id,
                                            bool success) {
  DCHECK(!ready_to_launch_);

  SYSLOG(INFO) << "OnFinishCrxInstall, id=" << extension_id
               << ", success=" << success;

  if (DidPrimaryOrSecondaryAppFailedToInstall(success, extension_id)) {
    install_observer_.RemoveAll();
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
    return;
  }

  // Wait for pending updates or dependent extensions to download.
  if (extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->pending_extension_manager()
          ->HasPendingExtensions()) {
    return;
  }

  install_observer_.RemoveAll();
  if (delegate_->IsShowingNetworkConfigScreen()) {
    SYSLOG(WARNING) << "Showing network config screen";
    return;
  }

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  if (!primary_app) {
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    OnLaunchFailure(KioskAppLaunchError::NOT_KIOSK_ENABLED);
    return;
  }

  if (!secondary_apps_installed_)
    MaybeInstallSecondaryApps();
  else
    MaybeCheckExtensionUpdate();
}

void StartupAppLauncher::OnKioskExtensionLoadedInCache(
    const std::string& app_id) {
  OnKioskAppDataLoadStatusChanged(app_id);
}

void StartupAppLauncher::OnKioskExtensionDownloadFailed(
    const std::string& app_id) {
  OnKioskAppDataLoadStatusChanged(app_id);
}

void StartupAppLauncher::OnKioskAppDataLoadStatusChanged(
    const std::string& app_id) {
  DCHECK(!ready_to_launch_);

  if (app_id != app_id_ || !wait_for_crx_update_)
    return;

  wait_for_crx_update_ = false;
  if (KioskAppManager::Get()->HasCachedCrx(app_id_))
    BeginInstall();
  else
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_DOWNLOAD);
}

bool StartupAppLauncher::IsAnySecondaryAppPending() const {
  const extensions::Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(extension);
  for (const auto& app : info->secondary_apps) {
    if (extensions::ExtensionSystem::Get(profile_)
            ->extension_service()
            ->pending_extension_manager()
            ->IsIdPending(app.id)) {
      return true;
    }
  }
  return false;
}

bool StartupAppLauncher::AreSecondaryAppsInstalled() const {
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

bool StartupAppLauncher::HasSecondaryApps() const {
  const extensions::Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  return extensions::KioskModeInfo::HasSecondaryApps(extension);
}

bool StartupAppLauncher::PrimaryAppHasPendingUpdate() const {
  return !!extensions::ExtensionSystem::Get(profile_)
               ->extension_service()
               ->GetPendingExtensionUpdate(app_id_);
}

bool StartupAppLauncher::DidPrimaryOrSecondaryAppFailedToInstall(
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

const extensions::Extension* StartupAppLauncher::GetPrimaryAppExtension()
    const {
  return extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
      app_id_);
}

void StartupAppLauncher::LaunchApp() {
  if (!ready_to_launch_) {
    NOTREACHED();
    SYSLOG(ERROR) << "LaunchApp() called but launcher is not initialized.";
  }

  const Extension* extension = GetPrimaryAppExtension();
  CHECK(extension);

  if (!extensions::KioskModeInfo::IsKioskEnabled(extension)) {
    OnLaunchFailure(KioskAppLaunchError::NOT_KIOSK_ENABLED);
    return;
  }

  SYSLOG(INFO) << "Attempt to launch app.";

  // Always open the app in a window.
  apps::LaunchService::Get(profile_)->OpenApplication(apps::AppLaunchParams(
      extension->id(), apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceKiosk));

  KioskAppManager::Get()->InitSession(profile_, app_id_);
  session_manager::SessionManager::Get()->SessionStarted();

  if (diagnostic_mode_)
    KioskDiagnosisRunner::Run(profile_, app_id_);

  OnLaunchSuccess();
}

void StartupAppLauncher::OnLaunchSuccess() {
  delegate_->OnLaunchSucceeded();
}

void StartupAppLauncher::OnLaunchFailure(KioskAppLaunchError::Error error) {
  SYSLOG(ERROR) << "App launch failed, error: " << error;
  DCHECK_NE(KioskAppLaunchError::NONE, error);

  delegate_->OnLaunchFailed(error);
}

void StartupAppLauncher::BeginInstall() {
  DCHECK(!ready_to_launch_);

  SYSLOG(INFO) << "BeginInstall";
  extensions::file_util::SetUseSafeInstallation(true);
  KioskAppManager::Get()->UpdatePrimaryAppLoaderPrefs(app_id_);
  if (extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->pending_extension_manager()
          ->IsIdPending(app_id_)) {
    delegate_->OnInstallingApp();
    // Observe the crx installation events.
    install_observer_.Add(
        extensions::InstallTrackerFactory::GetForBrowserContext(profile_));
    return;
  }

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  if (!primary_app) {
    // The extension is skipped for installation due to some error.
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    // The installed primary app is not kiosk enabled.
    OnLaunchFailure(KioskAppLaunchError::NOT_KIOSK_ENABLED);
    return;
  }

  // Install secondary apps.
  MaybeInstallSecondaryApps();
}

void StartupAppLauncher::MaybeInstallSecondaryApps() {
  if (ready_to_launch_)
    return;

  if (!AreSecondaryAppsInstalled() && !delegate_->IsNetworkReady()) {
    DelayNetworkCall(
        base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
        base::Bind(&StartupAppLauncher::MaybeInstallSecondaryApps,
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
    delegate_->OnInstallingApp();
    // Observe the crx installation events.
    install_observer_.Add(
        extensions::InstallTrackerFactory::GetForBrowserContext(profile_));
    return;
  }

  if (AreSecondaryAppsInstalled()) {
    // Check extension update before launching the primary kiosk app.
    MaybeCheckExtensionUpdate();
  } else {
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
  }
}

void StartupAppLauncher::OnReadyToLaunch() {
  DCHECK(ready_to_launch_);
  SYSLOG(INFO) << "Kiosk app is ready to launch.";
  MaybeUpdateAppData();
  delegate_->OnReadyToLaunch();
}

void StartupAppLauncher::MaybeUpdateAppData() {
  // Skip copying meta data from the current installed primary app when
  // there is a pending update.
  if (PrimaryAppHasPendingUpdate())
    return;

  KioskAppManager::Get()->ClearAppData(app_id_);
  KioskAppManager::Get()->UpdateAppDataFromProfile(app_id_, profile_, NULL);
}

}   // namespace chromeos
