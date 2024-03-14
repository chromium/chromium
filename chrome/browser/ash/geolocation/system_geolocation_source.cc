// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/geolocation/system_geolocation_source.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/branded_strings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

SystemGeolocationSource::SystemGeolocationSource()
    : permission_update_callback_(base::DoNothing()) {
  DCHECK(Shell::Get());
  DCHECK(Shell::Get()->session_controller());
  observer_.Observe(Shell::Get()->session_controller());
  PrefService* last_active_user_pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (last_active_user_pref_service) {
    OnActiveUserPrefServiceChanged(last_active_user_pref_service);
  }
}

SystemGeolocationSource::~SystemGeolocationSource() = default;

// static
std::unique_ptr<device::GeolocationSystemPermissionManager>
SystemGeolocationSource::CreateGeolocationSystemPermissionManagerOnAsh() {
  return std::make_unique<device::GeolocationSystemPermissionManager>(
      std::make_unique<SystemGeolocationSource>());
}

void SystemGeolocationSource::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = std::move(callback);
  if (pref_change_registrar_) {
    OnPrefChanged(prefs::kUserGeolocationAccessLevel);
  }
}

void SystemGeolocationSource::OpenSystemPermissionSetting() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(),
      chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath);
}

void SystemGeolocationSource::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Subscribing to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  // value might have changed, hence we trigger the update function
  OnPrefChanged(prefs::kUserGeolocationAccessLevel);
  pref_change_registrar_->Add(
      prefs::kUserGeolocationAccessLevel,
      base::BindRepeating(&SystemGeolocationSource::OnPrefChanged,
                          base::Unretained(this)));
}

void SystemGeolocationSource::OnPrefChanged(const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kUserGeolocationAccessLevel);
  DCHECK(pref_change_registrar_);
  // Get the actual permission status from CrOS by directly accessing pref
  // service.
  device::LocationSystemPermissionStatus status =
      device::LocationSystemPermissionStatus::kNotDetermined;

  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    PrefService* pref_service = pref_change_registrar_->prefs();
    if (pref_service) {
      status = (static_cast<GeolocationAccessLevel>(pref_service->GetInteger(
                    prefs::kUserGeolocationAccessLevel)) ==
                GeolocationAccessLevel::kAllowed)
                   ? device::LocationSystemPermissionStatus::kAllowed
                   : device::LocationSystemPermissionStatus::kDenied;
    }
  } else {
    // If the global switch feature is not enabled, we allow explicitly to be
    // backward compatible.
    status = device::LocationSystemPermissionStatus::kAllowed;
  }
  permission_update_callback_.Run(status);
}

}  // namespace ash
