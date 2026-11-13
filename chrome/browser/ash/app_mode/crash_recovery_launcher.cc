// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/crash_recovery_launcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/scoped_accelerator_disabler.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_service_launcher.h"
#include "components/session_manager/core/session_manager.h"

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
      return std::make_unique<KioskWebAppServiceLauncher>(
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

CrashRecoveryLauncher::CrashRecoveryLauncher(
    PrefService* local_state,
    Profile& profile,
    const KioskAppId& kiosk_app_id,
    std::unique_ptr<kiosk::AcceleratorController> accelerator_controller)
    : local_state_(CHECK_DEREF(local_state)),
      kiosk_app_id_(kiosk_app_id),
      profile_(profile),
      accelerator_controller_(std::move(accelerator_controller)) {
  CHECK(accelerator_controller_);
  app_launcher_ =
      CreateAppLauncher(profile, kiosk_app_id, /*network_delegate=*/this);
  observation_.Observe(app_launcher_.get());
}

CrashRecoveryLauncher::~CrashRecoveryLauncher() = default;

void CrashRecoveryLauncher::Start(OnDoneCallback callback) {
  done_callback_ = std::move(callback);
  SYSLOG(INFO) << "Starting crash recovery flow for app " << kiosk_app_id_;
  accelerator_disabler_ = std::make_unique<kiosk::ScopedAcceleratorDisabler>(
      *accelerator_controller_);
  app_launcher_->Initialize();
}

void CrashRecoveryLauncher::InvokeDoneCallback(
    bool success,
    const std::optional<webapps::AppId>& app_id) {
  if (done_callback_) {
    std::move(done_callback_).Run(success, app_id);
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

void CrashRecoveryLauncher::OnAppLaunching() {
  // We need to change the session state so we are able to create browser
  // windows.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
}

void CrashRecoveryLauncher::OnAppLaunched() {
  session_manager::SessionManager::Get()->SessionStarted();
}

void CrashRecoveryLauncher::OnAppWindowCreated(
    const std::optional<webapps::AppId>& app_id) {
  SYSLOG(INFO) << "Crash recovery flow succeeded";
  InvokeDoneCallback(true, app_id);
}

void CrashRecoveryLauncher::OnLaunchFailed(KioskAppLaunchError::Error error) {
  SYSLOG(WARNING) << "Crash recovery flow failed with error "
                  << static_cast<int>(error);
  KioskAppLaunchError::Save(local_state_.get(), error);
  InvokeDoneCallback(false, std::nullopt);
}

}  // namespace ash
