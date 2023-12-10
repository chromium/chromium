// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

void RecordKioskSecondaryAppsInstallResult(bool success) {
  base::UmaHistogramBoolean("Kiosk.SecondaryApps.InstallSuccessful", success);
}

}  // namespace

namespace chromeos {

ChromeKioskAppLauncher::ChromeKioskAppLauncher(Profile* profile,
                                               const std::string& app_id,
                                               bool network_available)
    : profile_(profile),
      app_id_(app_id),
      network_available_(network_available) {}

ChromeKioskAppLauncher::~ChromeKioskAppLauncher() = default;

void ChromeKioskAppLauncher::LaunchApp(LaunchCallback callback) {
  on_ready_callback_ = std::move(callback);

  const extensions::Extension* primary_app = GetPrimaryAppExtension();
  // Verify that required apps are installed. While the apps should be
  // present at this point, crash recovery flow skips app installation steps -
  // this means that the kiosk app might not yet be downloaded. If that is
  // the case, bail out from the app launch.
  if (!primary_app) {
    ReportLaunchFailure(LaunchResult::kUnableToLaunch);
    return;
  }

  if (!extensions::KioskModeInfo::IsKioskEnabled(primary_app)) {
    SYSLOG(WARNING) << "Kiosk app not kiosk enabled";
    ReportLaunchFailure(LaunchResult::kUnableToLaunch);
    return;
  }

  if (!AreSecondaryAppsInstalled()) {
    ReportLaunchFailure(LaunchResult::kUnableToLaunch);
    RecordKioskSecondaryAppsInstallResult(false);
    return;
  } else {
    extensions::KioskModeInfo* info =
        extensions::KioskModeInfo::Get(primary_app);
    if (!info->secondary_apps.empty()) {
      RecordKioskSecondaryAppsInstallResult(true);
    }
  }

  const bool offline_enabled =
      extensions::OfflineEnabledInfo::IsOfflineEnabled(primary_app);
  // If the app is not offline enabled, make sure the network is ready before
  // launching.
  if (!offline_enabled && !network_available_) {
    ReportLaunchFailure(LaunchResult::kNetworkMissing);
    return;
  }

  SetSecondaryAppsEnabledState(primary_app);
  MaybeUpdateAppData();

  const extensions::Extension* extension = GetPrimaryAppExtension();
  CHECK(extension);

  SYSLOG(INFO) << "Attempt to launch app.";

  app_service_launcher_ = std::make_unique<KioskAppServiceLauncher>(profile_);
  app_service_launcher_->CheckAndMaybeLaunchApp(
      extension->id(),
      base::BindOnce(&ChromeKioskAppLauncher::OnAppServiceAppLaunched,
                     weak_ptr_factory_.GetWeakPtr()));
  WaitForAppWindow();
}

void ChromeKioskAppLauncher::WaitForAppWindow() {
  auto* window_registry_ = extensions::AppWindowRegistry::Get(profile_);
  if (!window_registry_->GetAppWindowsForApp(app_id_).empty()) {
    ReportLaunchSuccess();
  } else {
    // Start waiting for app window.
    app_window_observation_.Observe(window_registry_);
  }
}

void ChromeKioskAppLauncher::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  if (app_window->extension_id() == app_id_) {
    app_window_observation_.Reset();
    ReportLaunchSuccess();
  }
}

void ChromeKioskAppLauncher::OnAppServiceAppLaunched(bool success) {
  if (!success) {
    ReportLaunchFailure(LaunchResult::kUnableToLaunch);
  }
}

void ChromeKioskAppLauncher::MaybeUpdateAppData() {
  // Skip copying meta data from the current installed primary app when
  // there is a pending update.
  if (PrimaryAppHasPendingUpdate()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::KioskChromeAppManager::Get()->ClearAppData(app_id_);
  ash::KioskChromeAppManager::Get()->UpdateAppDataFromProfile(app_id_, profile_,
                                                              nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeKioskAppLauncher::ReportLaunchSuccess() {
  SYSLOG(INFO) << "App launch completed";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  KioskSessionServiceLacros::Get()->InitChromeKioskSession(profile_, app_id_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::move(on_ready_callback_)
      .Run(ChromeKioskAppLauncher::LaunchResult::kSuccess);
}

void ChromeKioskAppLauncher::ReportLaunchFailure(
    ChromeKioskAppLauncher::LaunchResult error) {
  SYSLOG(ERROR) << "App launch failed, error: " << static_cast<int>(error);
  DCHECK_NE(ChromeKioskAppLauncher::LaunchResult::kSuccess, error);

  std::move(on_ready_callback_).Run(error);
}

const extensions::Extension* ChromeKioskAppLauncher::GetPrimaryAppExtension()
    const {
  return extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
      app_id_);
}

bool ChromeKioskAppLauncher::AreSecondaryAppsInstalled() const {
  const extensions::Extension& extension =
      CHECK_DEREF(GetPrimaryAppExtension());
  const auto& info = CHECK_DEREF(extensions::KioskModeInfo::Get(&extension));

  for (const auto& app : info.secondary_apps) {
    if (!extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
            app.id)) {
      return false;
    }
  }
  return true;
}

bool ChromeKioskAppLauncher::PrimaryAppHasPendingUpdate() const {
  return extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->GetPendingExtensionUpdate(app_id_);
}

void ChromeKioskAppLauncher::SetSecondaryAppsEnabledState(
    const extensions::Extension* primary_app) {
  const extensions::KioskModeInfo* info =
      extensions::KioskModeInfo::Get(primary_app);
  for (const auto& app_info : info->secondary_apps) {
    // If the enabled on launch is not specified in the manifest, the apps
    // enabled state should be kept as is.
    if (!app_info.enabled_on_launch.has_value()) {
      continue;
    }

    SetAppEnabledState(app_info.id, app_info.enabled_on_launch.value());
  }
}

void ChromeKioskAppLauncher::SetAppEnabledState(
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

}  // namespace chromeos
