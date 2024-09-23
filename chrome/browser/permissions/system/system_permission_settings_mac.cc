// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "chrome/browser/permissions/system/geolocation_observation.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

static_assert(BUILDFLAG(IS_MAC));

namespace system_permission_settings {

namespace {
bool denied(system_permission_settings::SystemPermission permission) {
  return system_permission_settings::SystemPermission::kDenied == permission ||
         system_permission_settings::SystemPermission::kRestricted ==
             permission;
}

bool prompt(system_permission_settings::SystemPermission permission) {
  return system_permission_settings::SystemPermission::kNotDetermined ==
         permission;
}
bool allowed(system_permission_settings::SystemPermission permission) {
  return system_permission_settings::SystemPermission::kAllowed == permission;
}

class PlatformHandleImpl : public PlatformHandle {
 public:
  bool CanPrompt(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        return prompt(
            system_permission_settings::CheckSystemVideoCapturePermission());
      case ContentSettingsType::MEDIASTREAM_MIC:
        return prompt(
            system_permission_settings::CheckSystemAudioCapturePermission());
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kNotDetermined;
      default:
        return false;
    }
  }

  bool IsDenied(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        return denied(
            system_permission_settings::CheckSystemVideoCapturePermission());
      case ContentSettingsType::MEDIASTREAM_MIC:
        return denied(
            system_permission_settings::CheckSystemAudioCapturePermission());
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kDenied;
      default:
        return false;
    }
  }

  bool IsAllowed(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        return allowed(
            system_permission_settings::CheckSystemVideoCapturePermission());
      case ContentSettingsType::MEDIASTREAM_MIC:
        return allowed(
            system_permission_settings::CheckSystemAudioCapturePermission());
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kAllowed;
      default:
        return true;
    }
  }

  void OpenSystemSettings(content::WebContents* web_contents,
                          ContentSettingsType type) override {
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
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM: {
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
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM: {
        system_permission_settings::RequestSystemVideoCapturePermission(
            std::move(callback));
        return;
      }
      case ContentSettingsType::MEDIASTREAM_MIC: {
        system_permission_settings::RequestSystemAudioCapturePermission(
            std::move(callback));
        return;
      }
      case ContentSettingsType::GEOLOCATION: {
        DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
        geolocation_callbacks_.push_back(std::move(callback));
        // The system permission prompt is modal and requires a user decision
        // (Allow or Deny) before it can be dismissed.
        if (geolocation_callbacks_.size() == 1u) {
          CHECK(!observation_);
          // Lazily setup geolocation status observation
          SystemPermissionChangedCallback clb = base::BindRepeating(
              &PlatformHandleImpl::OnSystemPermissionUpdated,
              weak_factory_.GetWeakPtr());
          observation_ = Observe(std::move(clb));
          CHECK_DEREF(device::GeolocationSystemPermissionManager::GetInstance())
              .RequestSystemPermission();
        }
        return;
      }
      default:
        std::move(callback).Run();
        NOTREACHED();
    }
  }

  std::unique_ptr<ScopedObservation> Observe(
      SystemPermissionChangedCallback observer) override {
    return std::make_unique<GeolocationObservation>(std::move(observer));
  }

 private:
  void OnSystemPermissionUpdated(ContentSettingsType content_type,
                                 bool /*is_blocked*/) {
    CHECK(content_type == ContentSettingsType::GEOLOCATION);
    // No further observation needed as all the current requests will now be
    // resolved
    observation_.reset();
    FlushGeolocationCallbacks();
  }

  void FlushGeolocationCallbacks() {
    auto callbacks = std::move(geolocation_callbacks_);
    for (auto& cb : callbacks) {
      std::move(cb).Run();
    }
  }

  std::vector<SystemPermissionResponseCallback> geolocation_callbacks_;
  std::unique_ptr<ScopedObservation> observation_;
  base::WeakPtrFactory<PlatformHandleImpl> weak_factory_{this};
};
}  // namespace

// static
std::unique_ptr<PlatformHandle> PlatformHandle::Create() {
  return std::make_unique<PlatformHandleImpl>();
}
}  // namespace system_permission_settings
