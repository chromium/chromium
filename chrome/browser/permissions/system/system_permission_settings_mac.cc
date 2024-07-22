// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <memory>

#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

static_assert(BUILDFLAG(IS_MAC));

namespace {
bool denied(system_media_permissions::SystemPermission permission) {
  return system_media_permissions::SystemPermission::kDenied == permission;
}

bool prompt(system_media_permissions::SystemPermission permission) {
  return system_media_permissions::SystemPermission::kNotDetermined ==
         permission;
}

bool allowed(system_media_permissions::SystemPermission permission) {
  return system_media_permissions::SystemPermission::kAllowed == permission;
}

}  // namespace

class SystemPermissionSettingsImpl
    : public SystemPermissionSettings,
      public device::GeolocationSystemPermissionManager::PermissionObserver {
 public:
  SystemPermissionSettingsImpl() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto* geolocation_system_permission_manager =
        device::GeolocationSystemPermissionManager::GetInstance();
    CHECK(geolocation_system_permission_manager);
    observation_.Observe(geolocation_system_permission_manager);
  }

  ~SystemPermissionSettingsImpl() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    FlushGeolocationCallbacks();
  }

  bool CanPrompt(ContentSettingsType type) const override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
        return prompt(
            system_media_permissions::CheckSystemVideoCapturePermission());
      case ContentSettingsType::MEDIASTREAM_MIC:
        return prompt(
            system_media_permissions::CheckSystemAudioCapturePermission());
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kNotDetermined;
      default:
        return false;
    }
  }

  bool IsDeniedImpl(ContentSettingsType type) const override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
        return denied(
            system_media_permissions::CheckSystemVideoCapturePermission());
      case ContentSettingsType::MEDIASTREAM_MIC:
        return denied(
            system_media_permissions::CheckSystemAudioCapturePermission());
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kDenied;
      default:
        return false;
    }
  }

  bool IsAllowedImpl(ContentSettingsType type) const override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
        return allowed(
            system_media_permissions::CheckSystemVideoCapturePermission());
      case ContentSettingsType::MEDIASTREAM_MIC:
        return allowed(
            system_media_permissions::CheckSystemAudioCapturePermission());
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kAllowed;
      default:
        return true;
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
      case ContentSettingsType::GEOLOCATION: {
        device::GeolocationSystemPermissionManager::GetInstance()
            ->OpenSystemPermissionSetting();
        return;
      }
      default:
        NOTREACHED();
    }
  }

  void Request(ContentSettingsType type,
               SystemPermissionResponseCallback callback) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA: {
        system_media_permissions::RequestSystemVideoCapturePermission(
            std::move(callback));
        return;
      }
      case ContentSettingsType::MEDIASTREAM_MIC: {
        system_media_permissions::RequestSystemAudioCapturePermission(
            std::move(callback));
        return;
      }
      case ContentSettingsType::GEOLOCATION: {
        geolocation_callbacks_.push_back(std::move(callback));
        // The system permission prompt is modal and requires a user decision
        // (Allow or Deny) before it can be dismissed.
        if (geolocation_callbacks_.size() == 1u) {
          device::GeolocationSystemPermissionManager::GetInstance()
              ->RequestSystemPermission();
        }
        return;
      }
      default:
        std::move(callback).Run();
        NOTREACHED();
    }
  }

  // device::GeolocationSystemPermissionManager::PermissionObserver
  // implementation.
  void OnSystemPermissionUpdated(
      device::LocationSystemPermissionStatus new_status) override {
    FlushGeolocationCallbacks();
  }

 private:
  void FlushGeolocationCallbacks() {
    auto callbacks = std::move(geolocation_callbacks_);
    for (auto& cb : callbacks) {
      std::move(cb).Run();
    }
  }

  std::vector<SystemPermissionResponseCallback> geolocation_callbacks_;
  base::ScopedObservation<
      device::GeolocationSystemPermissionManager,
      device::GeolocationSystemPermissionManager::PermissionObserver>
      observation_{this};
};

std::unique_ptr<SystemPermissionSettings>
SystemPermissionSettings::CreateImpl() {
  return std::make_unique<SystemPermissionSettingsImpl>();
}
