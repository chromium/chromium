// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system_permission_delegate_factory.h"

#include "base/notreached.h"
#include "chrome/browser/permissions/default_system_permission_delegate.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/permissions/camera_system_permission_delegate_mac.h"
#include "chrome/browser/permissions/geolocation_system_permission_delegate_mac.h"
#include "chrome/browser/permissions/microphone_system_permission_delegate_mac.h"
#endif

std::unique_ptr<EmbeddedPermissionPrompt::SystemPermissionDelegate>
SystemPermissionDelegateFactory::CreateSystemPermissionDelegate(
    ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::MEDIASTREAM_CAMERA:
#if BUILDFLAG(IS_MAC)
      return std::make_unique<CameraSystemPermissionDelegateMac>();
#else
      return std::make_unique<DefaultSystemPermissionDelegate>();
#endif
    case ContentSettingsType::GEOLOCATION:
#if BUILDFLAG(IS_MAC)
      return std::make_unique<GeolocationSystemPermissionDelegateMac>();
#else
      return std::make_unique<DefaultSystemPermissionDelegate>();
#endif
    case ContentSettingsType::MEDIASTREAM_MIC:
#if BUILDFLAG(IS_MAC)
      return std::make_unique<MicrophoneSystemPermissionDelegateMac>();
#else
      return std::make_unique<DefaultSystemPermissionDelegate>();
#endif
    default:
      NOTREACHED_NORETURN();
  }
}
