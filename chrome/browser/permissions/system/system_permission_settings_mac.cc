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
#include "base/task/thread_pool.h"
#include "chrome/browser/permissions/system/geolocation_observation.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#include "chrome/browser/permissions/system/system_media_permission_cache.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

static_assert(BUILDFLAG(IS_MAC));

namespace system_permission_settings {

namespace {

class PlatformHandleImpl : public PlatformHandle {
 public:
  PlatformHandleImpl()
      : media_cache_(
            base::BindOnce(&base::ThreadPool::CreateSequencedTaskRunner),
            base::BindRepeating(&CheckSystemVideoCapturePermission),
            base::BindRepeating(&CheckSystemAudioCapturePermission)) {}

  PlatformHandleImpl(const PlatformHandleImpl&) = delete;
  PlatformHandleImpl& operator=(const PlatformHandleImpl&) = delete;

  ~PlatformHandleImpl() override = default;

  // PlatformHandle:
  bool CanPrompt(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      case ContentSettingsType::MEDIASTREAM_MIC:
        return media_cache_.CanPrompt(type);
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kNotDetermined;
      case ContentSettingsType::CLIPBOARD_READ_WRITE:
        return IsSystemPermissionPrompt(
            system_permission_settings::CheckSystemClipboardPermission());
      default:
        return false;
    }
  }

  bool IsDenied(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      case ContentSettingsType::MEDIASTREAM_MIC:
        return media_cache_.IsDenied(type);
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kDenied;
      case ContentSettingsType::CLIPBOARD_READ_WRITE:
        return IsSystemPermissionDenied(
            system_permission_settings::CheckSystemClipboardPermission());
      default:
        return false;
    }
  }

  bool IsAllowed(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      case ContentSettingsType::MEDIASTREAM_MIC:
        return media_cache_.IsAllowed(type);
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kAllowed;
      case ContentSettingsType::CLIPBOARD_READ_WRITE:
        return IsSystemPermissionAllowed(
            system_permission_settings::CheckSystemClipboardPermission());
      default:
        return true;
    }
  }

  void IsDeniedFresh(ContentSettingsType type,
                     SystemPermissionDeniedCallback callback) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        media_cache_.IsDeniedFresh(type, std::move(callback));
        return;
      default:
        std::move(callback).Run(IsDenied(type));
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
      case ContentSettingsType::CLIPBOARD_READ_WRITE: {
        // Open Privacy & Security settings for clipboard/pasteboard permissions
        base::mac::OpenSystemSettingsPane(
            base::mac::SystemSettingsPane::kPrivacySecurity_PasteFromOtherApps);
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
            base::BindOnce(
                &PlatformHandleImpl::OnSystemPermissionRequestFinished,
                weak_factory_.GetWeakPtr(), type, std::move(callback)));
        return;
      }
      case ContentSettingsType::MEDIASTREAM_MIC: {
        system_permission_settings::RequestSystemAudioCapturePermission(
            base::BindOnce(
                &PlatformHandleImpl::OnSystemPermissionRequestFinished,
                weak_factory_.GetWeakPtr(), type, std::move(callback)));
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
      case ContentSettingsType::CLIPBOARD_READ_WRITE: {
        // Clipboard doesn't have a system permission request mechanism like
        // camera/microphone. The permission is granted when the app is first
        // used or can be managed in System Settings > Privacy & Security.
        // For now, just invoke the callback to indicate completion.
        std::move(callback).Run();
        return;
      }
      default:
        NOTREACHED();
    }
  }

  std::unique_ptr<ScopedObservation> Observe(
      SystemPermissionChangedCallback observer) override {
    return std::make_unique<GeolocationObservation>(std::move(observer));
  }

 private:
  void OnSystemPermissionRequestFinished(
      ContentSettingsType type,
      SystemPermissionResponseCallback callback) {
    media_cache_.RefreshSystemPermissionSettings(std::move(callback));
  }

  void OnSystemPermissionUpdated(ContentSettingsType content_type,
                                 bool /*is_blocked*/) {
    CHECK(content_type == ContentSettingsType::GEOLOCATION);
    observation_.reset();
    FlushGeolocationCallbacks();
  }

  void FlushGeolocationCallbacks() {
    auto callbacks = std::move(geolocation_callbacks_);
    for (auto& cb : callbacks) {
      std::move(cb).Run();
    }
  }

  SystemMediaPermissionCache media_cache_;
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
