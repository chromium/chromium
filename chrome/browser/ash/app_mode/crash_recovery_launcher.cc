// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/crash_recovery_launcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"

namespace ash {

namespace {
std::unique_ptr<KioskAppLauncher> CreateAppLauncher(
    Profile& profile,
    const KioskAppId& kiosk_app_id,
    KioskAppLauncher::NetworkDelegate* network_delegate) {
  switch (kiosk_app_id.type) {
    case KioskAppType::kChromeApp:
      return std::make_unique<StartupAppLauncher>(
          &profile, kiosk_app_id.app_id.value(), /*should_skip_install=*/true,
          network_delegate);
    case KioskAppType::kWebApp:
      return std::make_unique<WebKioskAppServiceLauncher>(
          &profile, kiosk_app_id.account_id, network_delegate);
    case KioskAppType::kIsolatedWebApp:
      return std::make_unique<KioskIwaLauncher>(
          &profile, kiosk_app_id.account_id, network_delegate);
    case KioskAppType::kArcvmApp:
      // TODO(crbug.com/418950200): Verify and fix behavior for ARCVM kiosk app
      // crash recovery.
      NOTREACHED();
  }

  NOTREACHED();
}
}  // namespace

CrashRecoveryLauncher::CrashRecoveryLauncher(Profile& profile,
                                             const KioskAppId& kiosk_app_id)
    : kiosk_app_id_(kiosk_app_id), profile_(profile) {
  app_launcher_ =
      CreateAppLauncher(profile, kiosk_app_id, /*network_delegate=*/this);
  observation_.Observe(app_launcher_.get());
}

CrashRecoveryLauncher::~CrashRecoveryLauncher() = default;

void CrashRecoveryLauncher::Start(OnDoneCallback callback) {
  done_callback_ = std::move(callback);
  SYSLOG(INFO) << "Starting crash recovery flow for app " << kiosk_app_id_;
  app_launcher_->Initialize();
}

void CrashRecoveryLauncher::InvokeDoneCallback(
    bool success,
    const std::optional<std::string>& app_name) {
  if (done_callback_) {
    std::move(done_callback_).Run(success, app_name);
  }
}

void CrashRecoveryLauncher::InitializeNetwork() {
  // This is on crash-restart path and assumes network is online.
  app_launcher_->ContinueWithNetworkReady();
}
bool CrashRecoveryLauncher::IsNetworkReady() const {
  // See comments above. Network is assumed to be online here.
  return true;
}

void CrashRecoveryLauncher::OnAppInstalling() {}

void CrashRecoveryLauncher::OnAppPrepared() {
  app_launcher_->LaunchApp();
}

void CrashRecoveryLauncher::OnAppLaunched() {}

void CrashRecoveryLauncher::OnAppWindowCreated(
    const std::optional<std::string>& app_name) {
  SYSLOG(INFO) << "Crash recovery flow succeeded";
  InvokeDoneCallback(true, app_name);
}

void CrashRecoveryLauncher::OnLaunchFailed(KioskAppLaunchError::Error error) {
  SYSLOG(WARNING) << "Crash recovery flow failed with error "
                  << static_cast<int>(error);
  KioskAppLaunchError::Save(error);
  InvokeDoneCallback(false, std::nullopt);
}

}  // namespace ash
