// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <memory>

#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

static_assert(BUILDFLAG(IS_MAC));

namespace {
bool adapt(system_media_permissions::SystemPermission permission) {
  return system_media_permissions::SystemPermission::kDenied == permission;
}

}  // namespace

class SystemPermissionSettingsImpl : public SystemPermissionSettings {
  bool IsPermissionDeniedImpl(ContentSettingsType type) const override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
        return adapt(
            system_media_permissions::CheckSystemVideoCapturePermission());
      case ContentSettingsType::MEDIASTREAM_MIC:
        return adapt(
            system_media_permissions::CheckSystemAudioCapturePermission());
      default:
        return false;
    }
  }

  void OpenSystemSettings(content::WebContents* web_contents,
                          ContentSettingsType type) const override {
    switch (type) {
      case ContentSettingsType::NOTIFICATIONS: {
        const webapps::AppId* app_id =
            web_app::WebAppTabHelper::GetAppId(web_contents);
        if (!app_id) {
          return;
        }
        base::mac::OpenSystemSettingsPane(
            base::mac::SystemSettingsPane::kNotifications,
            web_app::GetBundleIdentifierForShim(*app_id));
        return;
      }
      case ContentSettingsType::MEDIASTREAM_CAMERA: {
        base::mac::OpenSystemSettingsPane(
            base::mac::SystemSettingsPane::kPrivacySecurity_Camera);
        return;
      }
      case ContentSettingsType::MEDIASTREAM_MIC: {
        base::mac::OpenSystemSettingsPane(
            base::mac::SystemSettingsPane::kPrivacySecurity_Microphone);
        return;
      }
      default:
        NOTREACHED();
    }
  }
};

std::unique_ptr<SystemPermissionSettings> SystemPermissionSettings::Create() {
  return std::make_unique<SystemPermissionSettingsImpl>();
}
