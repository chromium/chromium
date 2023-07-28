// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/geolocation/system_geolocation_source.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/grit/chromium_strings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
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
std::unique_ptr<device::GeolocationManager>
SystemGeolocationSource::CreateGeolocationManagerOnAsh() {
  return std::make_unique<device::GeolocationManager>(
      std::make_unique<SystemGeolocationSource>());
}

void SystemGeolocationSource::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = std::move(callback);
  if (pref_change_registrar_) {
    OnPrefChanged(prefs::kUserGeolocationAllowed);
  }
}

void SystemGeolocationSource::TrackGeolocationAttempted() {
  // Use the default name for the browser.
  ash::privacy_hub_util::TrackGeolocationAttempted(
      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME));
}

void SystemGeolocationSource::TrackGeolocationRelinquished() {
  // Use the default name for the browser.
  ash::privacy_hub_util::TrackGeolocationRelinquished(
      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME));
}

void SystemGeolocationSource::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Subscribing to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  // value might have changed, hence we trigger the update function
  OnPrefChanged(prefs::kUserGeolocationAllowed);
  pref_change_registrar_->Add(
      prefs::kUserGeolocationAllowed,
      base::BindRepeating(&SystemGeolocationSource::OnPrefChanged,
                          base::Unretained(this)));
}

void SystemGeolocationSource::OnPrefChanged(const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kUserGeolocationAllowed);
  DCHECK(pref_change_registrar_);
  // Get the actual permission status from CrOS by directly accessing pref
  // service.
  device::LocationSystemPermissionStatus status =
      device::LocationSystemPermissionStatus::kNotDetermined;

  if (ash::features::IsCrosPrivacyHubV1Enabled()) {
    PrefService* pref_service = pref_change_registrar_->prefs();
    if (pref_service) {
      status = pref_service->GetBoolean(prefs::kUserGeolocationAllowed)
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
