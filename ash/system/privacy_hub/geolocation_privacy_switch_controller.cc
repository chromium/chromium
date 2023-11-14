// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

GeolocationPrivacySwitchController::GeolocationPrivacySwitchController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

GeolocationPrivacySwitchController::~GeolocationPrivacySwitchController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
GeolocationPrivacySwitchController* GeolocationPrivacySwitchController::Get() {
  PrivacyHubController* privacy_hub_controller =
      Shell::Get()->privacy_hub_controller();
  return privacy_hub_controller
             ? privacy_hub_controller->geolocation_controller()
             : nullptr;
}

void GeolocationPrivacySwitchController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUserGeolocationAccessLevel,
      base::BindRepeating(
          &GeolocationPrivacySwitchController::OnPreferenceChanged,
          base::Unretained(this)));
  UpdateNotification();
}

void GeolocationPrivacySwitchController::TrackGeolocationAttempted(
    const std::string& app_name) {
  ++usage_per_app_[app_name];
  ++usage_cnt_;
  UpdateNotification();
}

void GeolocationPrivacySwitchController::TrackGeolocationRelinquished(
    const std::string& app_name) {
  --usage_per_app_[app_name];
  --usage_cnt_;
  if (usage_per_app_[app_name] < 0 || usage_cnt_ < 0) {
    LOG(ERROR) << "Geolocation usage termination without start: count("
               << app_name << ") = " << usage_per_app_[app_name]
               << ", total count = " << usage_cnt_;
    NOTREACHED();
  }

  UpdateNotification();
}

std::vector<std::u16string> GeolocationPrivacySwitchController::GetActiveApps(
    size_t max_count) const {
  std::vector<std::u16string> apps;
  for (const auto& [name, cnt] : usage_per_app_) {
    if (cnt > 0) {
      apps.push_back(base::UTF8ToUTF16(name));
      if (apps.size() == max_count) {
        break;
      }
    }
  }
  return apps;
}

void GeolocationPrivacySwitchController::OnPreferenceChanged() {
  const GeolocationAccessLevel geolocation_state =
      static_cast<GeolocationAccessLevel>(
          pref_change_registrar_->prefs()->GetInteger(
              prefs::kUserGeolocationAccessLevel));
  VLOG(1) << "Privacy Hub: Geolocation switch state = "
          << static_cast<int>(geolocation_state);
  UpdateNotification();
}

bool GeolocationPrivacySwitchController::IsGeolocationUsageAllowedForApps() {
  GeolocationAccessLevel geolocation_access_level =
      static_cast<GeolocationAccessLevel>(
          pref_change_registrar_->prefs()->GetInteger(
              prefs::kUserGeolocationAccessLevel));

  switch (geolocation_access_level) {
    case GeolocationAccessLevel::kAllowed:
      return true;
    case GeolocationAccessLevel::kOnlyAllowedForSystem:
    case GeolocationAccessLevel::kDisallowed:
      return false;
  }
}

void GeolocationPrivacySwitchController::UpdateNotification() {
  if (!pref_change_registrar_ || !pref_change_registrar_->prefs()) {
    return;
  }

  PrivacyHubNotificationController* notification_controller =
      PrivacyHubNotificationController::Get();
  if (!notification_controller) {
    return;
  }

  if (usage_cnt_ == 0 || IsGeolocationUsageAllowedForApps()) {
    notification_controller->RemoveSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kLocation);
    return;
  }

  notification_controller->ShowSoftwareSwitchNotification(
      SensorDisabledNotificationDelegate::Sensor::kLocation);
}

}  // namespace ash
