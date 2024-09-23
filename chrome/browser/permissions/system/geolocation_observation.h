// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_GEOLOCATION_OBSERVATION_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_GEOLOCATION_OBSERVATION_H_

#include "base/scoped_observation.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

static_assert(BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED));

namespace system_permission_settings {

class GeolocationObservation
    : public ScopedObservation,
      public device::GeolocationSystemPermissionManager::PermissionObserver {
 public:
  explicit GeolocationObservation(SystemPermissionChangedCallback callback);
  ~GeolocationObservation() override;

  // device::GeolocationSystemPermissionManager::PermissionObserver:
  void OnSystemPermissionUpdated(
      device::LocationSystemPermissionStatus new_status) override;

 private:
  SystemPermissionChangedCallback callback_;
  base::ScopedObservation<
      device::GeolocationSystemPermissionManager,
      device::GeolocationSystemPermissionManager::PermissionObserver>
      observation_{this};
};

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_GEOLOCATION_OBSERVATION_H_
