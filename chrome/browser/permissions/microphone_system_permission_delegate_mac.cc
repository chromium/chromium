// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/microphone_system_permission_delegate_mac.h"

#include <utility>

#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

MicrophoneSystemPermissionDelegateMac::MicrophoneSystemPermissionDelegateMac() =
    default;

MicrophoneSystemPermissionDelegateMac::
    ~MicrophoneSystemPermissionDelegateMac() = default;

bool MicrophoneSystemPermissionDelegateMac::CanShowSystemPermissionPrompt() {
  return system_media_permissions::CheckSystemAudioCapturePermission() ==
         system_media_permissions::SystemPermission::kNotDetermined;
}

void MicrophoneSystemPermissionDelegateMac::RequestSystemPermission(
    SystemPermissionResponseCallback callback) {
  system_media_permissions::RequestSystemAudioCapturePermission(
      std::move(callback));
}

void MicrophoneSystemPermissionDelegateMac::ShowSystemPermissionSettingsView() {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Microphone);
}

bool MicrophoneSystemPermissionDelegateMac::IsSystemPermissionDenied() {
  return system_media_permissions::CheckSystemAudioCapturePermission() ==
         system_media_permissions::SystemPermission::kDenied;
}

bool MicrophoneSystemPermissionDelegateMac::IsSystemPermissionAllowed() {
  return system_media_permissions::CheckSystemAudioCapturePermission() ==
         system_media_permissions::SystemPermission::kAllowed;
}
