// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/geolocation_observation.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

static_assert(BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED));

namespace system_permission_settings {

GeolocationObservation::GeolocationObservation(
    SystemPermissionChangedCallback callback)
    : callback_(std::move(callback)) {
  auto* geolocation_manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  if (geolocation_manager) {
    // Geolocation manager may not be available in some scenarios (unit_tests).
    observation_.Observe(geolocation_manager);
  }
}

GeolocationObservation::~GeolocationObservation() = default;

// device::GeolocationSystemPermissionManager::PermissionObserver:
void GeolocationObservation::OnSystemPermissionUpdated(
    device::LocationSystemPermissionStatus new_status) {
  const bool is_blocked =
      (new_status == device::LocationSystemPermissionStatus::kDenied);
  callback_.Run(ContentSettingsType::GEOLOCATION, is_blocked);
}

}  // namespace system_permission_settings
