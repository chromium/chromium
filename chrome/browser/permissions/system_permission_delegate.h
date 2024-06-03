// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_PERMISSION_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_PERMISSION_DELEGATE_H_

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt.h"

// `SystemPermissionDelegate` defines the methods for query / request system
// permission or showing system permission setting page. This is to hide the
// implementation for each permission type and to accommodate the difference
// on each platforms. Also derived classes should provide some way to handle
// the response of RequestSystemPermission. For example,
// GeolocatoinSystemPermissionDelegate inherits from
// device::GeolocationSystemPermissionManager::PermissionObserver. Or like
// CameraSystemPermissionDelegate passes callback to get notified when a system
// permission request is resolved.
class EmbeddedPermissionPrompt::SystemPermissionDelegate {
 public:
  using SystemPermissionResponseCallback = base::OnceCallback<void()>;

  SystemPermissionDelegate() = default;
  virtual ~SystemPermissionDelegate() = default;
  SystemPermissionDelegate(const SystemPermissionDelegate&) = delete;
  SystemPermissionDelegate& operator=(const SystemPermissionDelegate&) = delete;

  virtual bool CanShowSystemPermissionPrompt() = 0;

  // Initiates a system permission request and invokes the provided callback
  // once the user's decision is made.
  virtual void RequestSystemPermission(
      SystemPermissionResponseCallback callback) = 0;
  virtual void ShowSystemPermissionSettingsView() = 0;
  virtual bool IsSystemPermissionDenied() = 0;
  virtual bool IsSystemPermissionAllowed() = 0;
};

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_PERMISSION_DELEGATE_H_
