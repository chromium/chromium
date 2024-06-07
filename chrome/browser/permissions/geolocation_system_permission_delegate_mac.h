// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_GEOLOCATION_SYSTEM_PERMISSION_DELEGATE_MAC_H_
#define CHROME_BROWSER_PERMISSIONS_GEOLOCATION_SYSTEM_PERMISSION_DELEGATE_MAC_H_

#include "base/scoped_observation.h"
#include "chrome/browser/permissions/system_permission_delegate.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

class GeolocationSystemPermissionDelegateMac
    : public EmbeddedPermissionPrompt::SystemPermissionDelegate,
      public device::GeolocationSystemPermissionManager::PermissionObserver {
 public:
  GeolocationSystemPermissionDelegateMac();
  ~GeolocationSystemPermissionDelegateMac() override;
  explicit GeolocationSystemPermissionDelegateMac(
      const SystemPermissionDelegate&) = delete;
  GeolocationSystemPermissionDelegateMac& operator=(
      const SystemPermissionDelegate&) = delete;

  // EmbeddedPermissionPrompt::SystemPermissionDelegate implementation.
  bool CanShowSystemPermissionPrompt() override;
  void RequestSystemPermission(
      SystemPermissionResponseCallback callback) override;
  void ShowSystemPermissionSettingsView() override;
  bool IsSystemPermissionDenied() override;
  bool IsSystemPermissionAllowed() override;

  // device::GeolocationSystemPermissionManager::PermissionObserver
  // implementation.
  void OnSystemPermissionUpdated(
      device::LocationSystemPermissionStatus new_status) override;

 private:
  void FlushCallbacks();

  device::LocationSystemPermissionStatus system_permission_ =
      device::LocationSystemPermissionStatus::kNotDetermined;
  std::vector<SystemPermissionResponseCallback> callbacks_;
  base::ScopedObservation<
      device::GeolocationSystemPermissionManager,
      device::GeolocationSystemPermissionManager::PermissionObserver>
      observation_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_GEOLOCATION_SYSTEM_PERMISSION_DELEGATE_MAC_H_
