// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/startup_app_launcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"

using chromeos::ChromeKioskAppInstaller;
using chromeos::ChromeKioskAppLauncher;

namespace ash {

namespace {

const int kMaxLaunchAttempt = 5;

// Reduced backoff policy for extension downloader while Kiosk is launching.
const net::BackoffEntry::Policy kKioskLaunchExtensionBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = 2000,
    .multiply_factor = 2,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = 3000,
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false,
};

}  // namespace

StartupAppLauncher::StartupAppLauncher(
    Profile* profile,
    const std::string& app_id,
    bool should_skip_install,
    StartupAppLauncher::NetworkDelegate* network_delegate)
    : KioskAppLauncher(network_delegate),
      profile_(profile),
      app_id_(app_id),
      should_skip_install_(should_skip_install) {
  CHECK(profile_);
  DCHECK(crx_file::id_util::IdIsValid(app_id_));

  // Reduce extension downloader retry backoff to avoid waiting on splash screen
  // for a long time.
  KioskChromeAppManager::Get()->SetExtensionDownloaderBackoffPolicy(
      kKioskLaunchExtensionBackoffPolicy);
}

StartupAppLauncher::~StartupAppLauncher() {
  // Restore to default extension downloader backoff policy.
  KioskChromeAppManager::Get()->SetExtensionDownloaderBackoffPolicy(
      std::nullopt);
}

void StartupAppLauncher::AddObserver(KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void StartupAppLauncher::RemoveObserver(KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void StartupAppLauncher::Initialize() {
  CHECK(state_ != LaunchState::kReadyToLaunch &&
        state_ != LaunchState::kWaitingForWindow &&
        state_ != LaunchState::kLaunchSucceeded);

  if (should_skip_install_) {
    OnInstallSuccess();
    return;
  }

  // Update the offline enabled crx cache if the network is ready;
  // or just install the app.
  if (delegate_->IsNetworkReady()) {
    ContinueWithNetworkReady();
  } else {
    BeginInstall();
  }
}

void StartupAppLauncher::ContinueWithNetworkReady() {
  SYSLOG(INFO) << "ContinueWithNetworkReady"
               << ", state_=" << base::to_underlying(state_);

  if (state_ != LaunchState::kInitializingNetwork &&
      state_ != LaunchState::kNotStarted) {
    return;
  }

  if (should_skip_install_) {
    OnInstallSuccess();
    return;
  }

  // The network might not be ready when KioskChromeAppManager tries to update
  // external cache initially. Update the external cache now that the network
  // is ready for sure.
  state_ = LaunchState::kWaitingForCache;
  kiosk_app_manager_observation_.Observe(KioskChromeAppManager::Get());
  KioskChromeAppManager::Get()->UpdateExternalCache();
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
  CHECK_EQ(state_, LaunchState::kWaitingForCache);

  if (app_id != app_id_) {
    return;
  }

  kiosk_app_manager_observation_.Reset();

  if (KioskChromeAppManager::Get()->HasCachedCrx(app_id_)) {
    BeginInstall();
  } else {
    OnLaunchFailure(KioskAppLaunchError::Error::kUnableToDownload);
  }
}

void StartupAppLauncher::BeginInstall() {
  state_ = LaunchState::kInstallingApp;
  observers_.NotifyAppInstalling();
  installer_ = std::make_unique<ChromeKioskAppInstaller>(
      profile_,
      KioskChromeAppManager::Get()->CreatePrimaryAppInstallData(app_id_));
  installer_->BeginInstall(base::BindOnce(
      &StartupAppLauncher::OnInstallComplete, weak_ptr_factory_.GetWeakPtr()));
}

void StartupAppLauncher::OnInstallComplete(
    ChromeKioskAppInstaller::InstallResult result) {
  CHECK_EQ(state_, LaunchState::kInstallingApp);

  installer_.reset();

  switch (result) {
    case ChromeKioskAppInstaller::InstallResult::kSuccess:
      OnInstallSuccess();
      return;
    case ChromeKioskAppInstaller::InstallResult::kPrimaryAppInstallFailed:
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToInstall);
      return;
    case ChromeKioskAppInstaller::InstallResult::kPrimaryAppNotKioskEnabled:
      OnLaunchFailure(KioskAppLaunchError::Error::kNotKioskEnabled);
      return;
    case ChromeKioskAppInstaller::InstallResult::kPrimaryAppNotCached:
    case ChromeKioskAppInstaller::InstallResult::kSecondaryAppInstallFailed:
      if (!RetryWhenNetworkIsAvailable()) {
        OnLaunchFailure(KioskAppLaunchError::Error::kUnableToInstall);
      }
      return;
    case ChromeKioskAppInstaller::InstallResult::kUnknown:
      SYSLOG(ERROR) << "Received unknown InstallResult";
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToInstall);
      return;
  }
}

void StartupAppLauncher::OnInstallSuccess() {
  state_ = LaunchState::kReadyToLaunch;

  observers_.NotifyAppPrepared();
}

void StartupAppLauncher::LaunchApp() {
  if (state_ != LaunchState::kReadyToLaunch) {
    NOTREACHED_IN_MIGRATION();
    SYSLOG(ERROR) << "LaunchApp() called but launcher is not initialized.";
  }

  launcher_ = std::make_unique<ChromeKioskAppLauncher>(
      profile_, app_id_, delegate_->IsNetworkReady());

  launcher_->LaunchApp(base::BindOnce(&StartupAppLauncher::OnLaunchComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void StartupAppLauncher::OnLaunchComplete(
    ChromeKioskAppLauncher::LaunchResult result) {
  CHECK_EQ(state_, LaunchState::kReadyToLaunch);

  launcher_.reset();

  switch (result) {
    case ChromeKioskAppLauncher::LaunchResult::kSuccess:
      OnLaunchSuccess();
      return;
    case ChromeKioskAppLauncher::LaunchResult::kUnableToLaunch:
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      return;
    case ChromeKioskAppLauncher::LaunchResult::kNetworkMissing:
      if (!RetryWhenNetworkIsAvailable()) {
        OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      }
      return;
    case ChromeKioskAppLauncher::LaunchResult::kUnknown:
      SYSLOG(ERROR) << "Received unknown LaunchResult";
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      return;
  }
}

void StartupAppLauncher::OnLaunchSuccess() {
  state_ = LaunchState::kLaunchSucceeded;
  observers_.NotifyAppLaunched();
  observers_.NotifyAppWindowCreated();
}

void StartupAppLauncher::OnLaunchFailure(KioskAppLaunchError::Error error) {
  SYSLOG(ERROR) << "App launch failed, error: " << static_cast<int>(error);
  CHECK_NE(KioskAppLaunchError::Error::kNone, error);

  observers_.NotifyLaunchFailed(error);
}

}  // namespace ash
