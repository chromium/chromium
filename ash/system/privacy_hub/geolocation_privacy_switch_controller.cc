// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "base/notreached.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

GeolocationPrivacySwitchController::GeolocationPrivacySwitchController()
    : session_observation_(this) {
  session_observation_.Observe(Shell::Get()->session_controller());
}

GeolocationPrivacySwitchController::~GeolocationPrivacySwitchController() =
    default;

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

  // Establish the initial value for the cached_access_level_.
  cached_access_level_ = static_cast<GeolocationAccessLevel>(
      pref_change_registrar_->prefs()->GetInteger(
          prefs::kUserGeolocationAccessLevel));

  if (features::IsCrosPrivacyHubLocationEnabled()) {
    UpdateNotification();
  }
}

void GeolocationPrivacySwitchController::OnPreferenceChanged() {
  VLOG(1) << "Privacy Hub: Geolocation switch state = "
          << static_cast<int>(AccessLevel());
  if (features::IsCrosPrivacyHubLocationEnabled()) {
    CHECK(pref_change_registrar_);
    const GeolocationAccessLevel new_access_level =
        static_cast<GeolocationAccessLevel>(
            pref_change_registrar_->prefs()->GetInteger(
                prefs::kUserGeolocationAccessLevel));

    CHECK(cached_access_level_.has_value());
    if (new_access_level != *cached_access_level_) {
      // update the pref that tracks the previous access level.
      pref_change_registrar_->prefs()->SetInteger(
          prefs::kUserPreviousGeolocationAccessLevel,
          static_cast<int>(*cached_access_level_));
      cached_access_level_ = new_access_level;
    }
    UpdateNotification();
  } else {
    // Feature disabled means geolocation is always allowed
    CHECK(pref_change_registrar_);
    pref_change_registrar_->prefs()->SetInteger(
        prefs::kUserGeolocationAccessLevel,
        static_cast<int>(GeolocationAccessLevel::kAllowed));
  }
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

bool GeolocationPrivacySwitchController::IsGeolocationUsageAllowedForApps() {
  switch (AccessLevel()) {
    case GeolocationAccessLevel::kAllowed:
      return true;
    case GeolocationAccessLevel::kOnlyAllowedForSystem:
    case GeolocationAccessLevel::kDisallowed:
      return false;
  }
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

GeolocationAccessLevel GeolocationPrivacySwitchController::AccessLevel() const {
  CHECK(cached_access_level_.has_value());
  return *cached_access_level_;
}

GeolocationAccessLevel GeolocationPrivacySwitchController::PreviousAccessLevel()
    const {
  CHECK(pref_change_registrar_);
  const GeolocationAccessLevel previous_level =
      static_cast<GeolocationAccessLevel>(
          pref_change_registrar_->prefs()->GetInteger(
              prefs::kUserPreviousGeolocationAccessLevel));
  // Previous level should be distinct.
  CHECK_NE(previous_level, AccessLevel());
  return previous_level;
}

void GeolocationPrivacySwitchController::SetAccessLevel(
    GeolocationAccessLevel access_level) {
  if (!features::IsCrosPrivacyHubLocationEnabled()) {
    return;
  }
  CHECK(pref_change_registrar_);
  pref_change_registrar_->prefs()->SetInteger(
      prefs::kUserGeolocationAccessLevel, static_cast<int>(access_level));
}

void GeolocationPrivacySwitchController::UpdateNotification() {
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

void GeolocationPrivacySwitchController::ApplyArcLocationUpdate(
    bool geolocation_enabled) {
  if (!features::IsCrosPrivacyHubLocationEnabled()) {
    return;
  }
  if (geolocation_enabled &&
      AccessLevel() != GeolocationAccessLevel::kAllowed) {
    SetAccessLevel(GeolocationAccessLevel::kAllowed);
  } else if (!geolocation_enabled &&
             AccessLevel() == ash::GeolocationAccessLevel::kAllowed) {
    // Restore previous location level, which is blocking.
    SetAccessLevel(PreviousAccessLevel());
  }
}

}  // namespace ash
