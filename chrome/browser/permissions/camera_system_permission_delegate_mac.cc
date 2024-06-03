// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/camera_system_permission_delegate_mac.h"

#include <utility>

#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

CameraSystemPermissionDelegateMac::CameraSystemPermissionDelegateMac() =
    default;

CameraSystemPermissionDelegateMac::~CameraSystemPermissionDelegateMac() =
    default;

bool CameraSystemPermissionDelegateMac::CanShowSystemPermissionPrompt() {
  return system_media_permissions::CheckSystemVideoCapturePermission() ==
         system_media_permissions::SystemPermission::kNotDetermined;
}

void CameraSystemPermissionDelegateMac::RequestSystemPermission(
    SystemPermissionResponseCallback callback) {
  system_media_permissions::RequestSystemVideoCapturePermission(
      std::move(callback));
}

void CameraSystemPermissionDelegateMac::ShowSystemPermissionSettingsView() {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Camera);
}

bool CameraSystemPermissionDelegateMac::IsSystemPermissionDenied() {
  return system_media_permissions::CheckSystemVideoCapturePermission() ==
         system_media_permissions::SystemPermission::kDenied;
}

bool CameraSystemPermissionDelegateMac::IsSystemPermissionAllowed() {
  return system_media_permissions::CheckSystemVideoCapturePermission() ==
         system_media_permissions::SystemPermission::kAllowed;
}
