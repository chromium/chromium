// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/startup_app_launcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/app_window/app_window.h"
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

namespace ash {

namespace {

const int kMaxLaunchAttempt = 5;

}  // namespace

StartupAppLauncher::StartupAppLauncher(Profile* profile,
                                       const std::string& app_id,
                                       StartupAppLauncher::Delegate* delegate)
    : KioskAppLauncher(delegate), profile_(profile), app_id_(app_id) {
  DCHECK(profile_);
  DCHECK(crx_file::id_util::IdIsValid(app_id_));
}

StartupAppLauncher::~StartupAppLauncher() {
  if (state_ == LaunchState::kWaitingForWindow)
    window_registry_->RemoveObserver(this);
}

void StartupAppLauncher::Initialize() {
  MaybeInitializeNetwork();
}

void StartupAppLauncher::ContinueWithNetworkReady() {
  SYSLOG(INFO) << "ContinueWithNetworkReady"
               << ", state_="
               << static_cast<typename std::underlying_type<LaunchState>::type>(
                      state_);

  if (state_ != LaunchState::kInitializingNetwork)
    return;

  if (delegate_->ShouldSkipAppInstallation()) {
    OnInstallSuccess();
    return;
  }

  // The network might not be ready when KioskAppManager tries to update
  // external cache initially. Update the external cache now that the network
  // is ready for sure.
  state_ = LaunchState::kWaitingForCache;
  kiosk_app_manager_observation_.Observe(KioskAppManager::Get());
  KioskAppManager::Get()->UpdateExternalCache();
}

void StartupAppLauncher::RestartLauncher() {
  // Do not allow restarts after the launcher finishes kiosk apps installation -
  // notify the delegate that kiosk app is ready to launch, in case the launch
  // was delayed, for example by network config dialog.
  if (state_ == LaunchState::kReadyToLaunch) {
    delegate_->OnAppPrepared();
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
  DCHECK(state_ != LaunchState::kReadyToLaunch &&
         state_ != LaunchState::kWaitingForWindow &&
         state_ != LaunchState::kLaunchSucceeded);

  const Extension* extension = GetPrimaryAppExtension();
  bool crx_cached = KioskAppManager::Get()->HasCachedCrx(app_id_);
  const bool requires_network =
      (!extension && !crx_cached) ||
      (extension &&
       !extensions::OfflineEnabledInfo::IsOfflineEnabled(extension));

  SYSLOG(INFO) << "MaybeInitializeNetwork"
               << ", requires_network=" << requires_network
               << ", network_ready=" << delegate_->IsNetworkReady();

  state_ = LaunchState::kInitializingNetwork;

  if (requires_network) {
    delegate_->InitializeNetwork();
    return;
  }

  if (delegate_->ShouldSkipAppInstallation()) {
    OnInstallSuccess();
    return;
  }

  // Update the offline enabled crx cache if the network is ready;
  // or just install the app.
  if (delegate_->IsNetworkReady())
    ContinueWithNetworkReady();
  else
    BeginInstall();
}

bool StartupAppLauncher::RetryWhenNetworkIsAvailable() {
  ++launch_attempt_;
  if (launch_attempt_ < kMaxLaunchAttempt) {
    state_ = LaunchState::kInitializingNetwork;
    delegate_->InitializeNetwork();
    return true;
  }
  return false;
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
  if (state_ != LaunchState::kWaitingForCache)
    return;

  if (app_id != app_id_)
    return;

  if (KioskAppManager::Get()->HasCachedCrx(app_id_))
    BeginInstall();
  else
    OnLaunchFailure(KioskAppLaunchError::Error::kUnableToDownload);
}

const extensions::Extension* StartupAppLauncher::GetPrimaryAppExtension()
    const {
  return extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
      app_id_);
}

void StartupAppLauncher::BeginInstall() {
  state_ = LaunchState::kInstallingApp;
  delegate_->OnAppInstalling();
  installer_ = std::make_unique<ChromeKioskAppInstaller>(
      profile_, KioskAppManager::Get()->CreatePrimaryAppInstallData(app_id_),
      delegate_);
  installer_->BeginInstall(base::BindOnce(
      &StartupAppLauncher::OnInstallComplete, weak_ptr_factory_.GetWeakPtr()));
}

void StartupAppLauncher::OnInstallComplete(
    ChromeKioskAppInstaller::InstallResult result) {
  DCHECK(state_ == LaunchState::kInstallingApp);

  installer_.reset();

  switch (result) {
    case ChromeKioskAppInstaller::InstallResult::kSuccess:
      OnInstallSuccess();
      return;
    case ChromeKioskAppInstaller::InstallResult::kUnableToInstall:
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToInstall);
      return;
    case ChromeKioskAppInstaller::InstallResult::kNotKioskEnabled:
      OnLaunchFailure(KioskAppLaunchError::Error::kNotKioskEnabled);
      return;
    case ChromeKioskAppInstaller::InstallResult::kNetworkMissing:
      if (!RetryWhenNetworkIsAvailable())
        OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      return;
  }
}

void StartupAppLauncher::OnInstallSuccess() {
  state_ = LaunchState::kReadyToLaunch;
  // Updates to cached primary app crx will be ignored after this point, so
  // there is no need to observe the kiosk app manager any longer.
  kiosk_app_manager_observation_.Reset();
  delegate_->OnAppPrepared();
}

void StartupAppLauncher::LaunchApp() {
  if (state_ != LaunchState::kReadyToLaunch) {
    NOTREACHED();
    SYSLOG(ERROR) << "LaunchApp() called but launcher is not initialized.";
  }

  launcher_ = std::make_unique<ChromeKioskAppLauncher>(
      profile_, app_id_, delegate_->IsNetworkReady());

  launcher_->LaunchApp(base::BindOnce(&StartupAppLauncher::OnLaunchComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void StartupAppLauncher::OnLaunchComplete(
    ChromeKioskAppLauncher::LaunchResult result) {
  DCHECK(state_ == LaunchState::kReadyToLaunch);

  launcher_.reset();

  switch (result) {
    case ChromeKioskAppLauncher::LaunchResult::kSuccess:
      KioskAppManager::Get()->InitSession(profile_, app_id_);
      OnLaunchSuccess();
      return;
    case ChromeKioskAppLauncher::LaunchResult::kUnableToLaunch:
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      return;
    case ChromeKioskAppLauncher::LaunchResult::kNetworkMissing:
      if (!RetryWhenNetworkIsAvailable())
        OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      return;
  }
}

void StartupAppLauncher::OnLaunchSuccess() {
  delegate_->OnAppLaunched();
  state_ = LaunchState::kWaitingForWindow;

  window_registry_ = extensions::AppWindowRegistry::Get(profile_);
  // Start waiting for app window.
  if (!window_registry_->GetAppWindowsForApp(app_id_).empty()) {
    delegate_->OnAppWindowCreated();
    state_ = LaunchState::kLaunchSucceeded;
    return;
  } else {
    window_registry_->AddObserver(this);
  }
}

void StartupAppLauncher::OnAppWindowAdded(extensions::AppWindow* app_window) {
  if (app_window->extension_id() == app_id_) {
    state_ = LaunchState::kLaunchSucceeded;
    window_registry_->RemoveObserver(this);
    delegate_->OnAppWindowCreated();
  }
}

void StartupAppLauncher::OnLaunchFailure(KioskAppLaunchError::Error error) {
  SYSLOG(ERROR) << "App launch failed, error: " << static_cast<int>(error);
  DCHECK_NE(KioskAppLaunchError::Error::kNone, error);

  delegate_->OnLaunchFailed(error);
}

}  // namespace ash
