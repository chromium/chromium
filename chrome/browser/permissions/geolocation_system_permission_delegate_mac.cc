// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/geolocation_system_permission_delegate_mac.h"

#include "content/public/browser/browser_thread.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

GeolocationSystemPermissionDelegateMac::
    GeolocationSystemPermissionDelegateMac() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* geolocation_system_permission_manager =
      device::GeolocationSystemPermissionManager::GetInstance();
  CHECK(geolocation_system_permission_manager);
  observation_.Observe(geolocation_system_permission_manager);
  system_permission_ =
      geolocation_system_permission_manager->GetSystemPermission();
}

GeolocationSystemPermissionDelegateMac::
    ~GeolocationSystemPermissionDelegateMac() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FlushCallbacks();
}

bool GeolocationSystemPermissionDelegateMac::CanShowSystemPermissionPrompt() {
  return system_permission_ ==
         device::LocationSystemPermissionStatus::kNotDetermined;
}

void GeolocationSystemPermissionDelegateMac::RequestSystemPermission(
    SystemPermissionResponseCallback callback) {
  callbacks_.push_back(std::move(callback));
  // The system permission prompt is modal and requires a user decision (Allow
  // or Deny) before it can be dismissed.
  if (callbacks_.size() == 1u) {
    device::GeolocationSystemPermissionManager::GetInstance()
        ->RequestSystemPermission();
  }
}

void GeolocationSystemPermissionDelegateMac::
    ShowSystemPermissionSettingsView() {
  device::GeolocationSystemPermissionManager::GetInstance()
      ->OpenSystemPermissionSetting();
}

bool GeolocationSystemPermissionDelegateMac::IsSystemPermissionDenied() {
  return system_permission_ == device::LocationSystemPermissionStatus::kDenied;
}

bool GeolocationSystemPermissionDelegateMac::IsSystemPermissionAllowed() {
  return system_permission_ == device::LocationSystemPermissionStatus::kAllowed;
}

void GeolocationSystemPermissionDelegateMac::OnSystemPermissionUpdated(
    device::LocationSystemPermissionStatus new_status) {
  system_permission_ = new_status;
  FlushCallbacks();
}

void GeolocationSystemPermissionDelegateMac::FlushCallbacks() {
  auto callbacks = std::move(callbacks_);
  for (auto& cb : callbacks) {
    std::move(cb).Run();
  }
}
