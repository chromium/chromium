// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_web_app_launcher_base.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace ash {

KioskWebAppLauncherBase::KioskWebAppLauncherBase(
    Profile* profile,
    const AccountId& account_id,
    KioskAppLauncher::NetworkDelegate* network_delegate)
    : KioskAppLauncher(network_delegate),
      profile_(profile),
      account_id_(account_id) {}

KioskWebAppLauncherBase::~KioskWebAppLauncherBase() = default;

void KioskWebAppLauncherBase::AddObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void KioskWebAppLauncherBase::RemoveObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void KioskWebAppLauncherBase::Initialize() {
  InitAppServiceLauncher();
}

void KioskWebAppLauncherBase::ContinueWithNetworkReady() {
  observers().NotifyAppInstalling();
}

void KioskWebAppLauncherBase::LaunchApp() {
  app_service_launcher().CheckAndMaybeLaunchApp(
      GetInstalledWebAppId(),
      base::BindOnce(&KioskWebAppLauncherBase::OnAppLaunched,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&KioskWebAppLauncherBase::OnAppBecomesVisible,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskWebAppLauncherBase::NotifyAppPrepared() {
  observers().NotifyAppPrepared();
}

void KioskWebAppLauncherBase::NotifyLaunchFailed(
    KioskAppLaunchError::Error error) {
  observers().NotifyLaunchFailed(error);
}

void KioskWebAppLauncherBase::OnAppLaunched(bool success) {
  if (!success) {
    observers().NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
    return;
  }
  observers().NotifyAppLaunched();
}

void KioskWebAppLauncherBase::OnAppBecomesVisible() {
  observers().NotifyAppWindowCreated(
      web_app::GenerateApplicationNameFromAppId(GetInstalledWebAppId()));
}

void KioskWebAppLauncherBase::InitAppServiceLauncher() {
  CHECK(!app_service_launcher_);
  app_service_launcher_ =
      std::make_unique<chromeos::KioskAppServiceLauncher>(profile_);

  app_service_launcher_->EnsureAppTypeInitialized(
      apps::AppType::kWeb,
      base::BindOnce(&KioskWebAppLauncherBase::OnWebAppInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskWebAppLauncherBase::OnWebAppInitialized() {
  CheckAppInstallState();
}

}  // namespace ash
