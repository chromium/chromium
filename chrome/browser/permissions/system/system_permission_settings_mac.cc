// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/permissions/system/geolocation_observation.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

static_assert(BUILDFLAG(IS_MAC));

namespace system_permission_settings {

namespace {
bool IsSystemPermissionDenied(
    system_permission_settings::SystemPermission permission) {
  return system_permission_settings::SystemPermission::kDenied == permission ||
         system_permission_settings::SystemPermission::kRestricted ==
             permission;
}

bool IsSystemPermissionPrompt(
    system_permission_settings::SystemPermission permission) {
  return system_permission_settings::SystemPermission::kNotDetermined ==
         permission;
}
bool IsSystemPermissionAllowed(
    system_permission_settings::SystemPermission permission) {
  return system_permission_settings::SystemPermission::kAllowed == permission;
}

struct SystemPermissionState {
  SystemPermission camera = SystemPermission::kNotDetermined;
  SystemPermission mic = SystemPermission::kNotDetermined;
};

class PlatformHandleImpl : public PlatformHandle,
                           public BrowserCollectionObserver {
 public:
  PlatformHandleImpl() {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
    if (base::SequencedTaskRunner::HasCurrentDefault()) {
      RefreshSystemPermissionSettings();
    }
  }

  PlatformHandleImpl(const PlatformHandleImpl&) = delete;
  PlatformHandleImpl& operator=(const PlatformHandleImpl&) = delete;

  ~PlatformHandleImpl() override = default;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override {
    // When the browser window is activated, it's a good opportunity to check
    // system permission settings again, since the settings would very likely be
    // changed while the browser windows is not active.
    RefreshSystemPermissionSettings();
  }

  // PlatformHandle:
  bool CanPrompt(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        return IsSystemPermissionPrompt(
            GetCachedSystemPermission(ContentSettingsType::MEDIASTREAM_CAMERA));
      case ContentSettingsType::MEDIASTREAM_MIC:
        return IsSystemPermissionPrompt(
            GetCachedSystemPermission(ContentSettingsType::MEDIASTREAM_MIC));
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
        return IsSystemPermissionDenied(
            GetCachedSystemPermission(ContentSettingsType::MEDIASTREAM_CAMERA));
      case ContentSettingsType::MEDIASTREAM_MIC:
        return IsSystemPermissionDenied(
            GetCachedSystemPermission(ContentSettingsType::MEDIASTREAM_MIC));
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
        return IsSystemPermissionAllowed(
            GetCachedSystemPermission(ContentSettingsType::MEDIASTREAM_CAMERA));
      case ContentSettingsType::MEDIASTREAM_MIC:
        return IsSystemPermissionAllowed(
            GetCachedSystemPermission(ContentSettingsType::MEDIASTREAM_MIC));
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
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(
                &system_permission_settings::CheckSystemAudioCapturePermission),
            base::BindOnce(&PlatformHandleImpl::ProcessDeniedPermissionResult,
                           weak_factory_.GetWeakPtr(), type)
                .Then(std::move(callback)));
        return;
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(
                &system_permission_settings::CheckSystemVideoCapturePermission),
            base::BindOnce(&PlatformHandleImpl::ProcessDeniedPermissionResult,
                           weak_factory_.GetWeakPtr(), type)
                .Then(std::move(callback)));
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
  void RefreshSystemPermissionSettings(
      base::OnceClosure callback = base::DoNothing()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce([]() {
          return SystemPermissionState{
              system_permission_settings::CheckSystemVideoCapturePermission(),
              system_permission_settings::CheckSystemAudioCapturePermission()};
        }),
        base::BindOnce(&PlatformHandleImpl::OnSystemPermissionSettingsRefreshed,
                       weak_factory_.GetWeakPtr())
            .Then(std::move(callback)));
  }

  void OnSystemPermissionSettingsRefreshed(SystemPermissionState state) {
    UpdateSystemPermissionCacheEntry(ContentSettingsType::MEDIASTREAM_CAMERA,
                                     state.camera);
    UpdateSystemPermissionCacheEntry(ContentSettingsType::MEDIASTREAM_MIC,
                                     state.mic);
  }

  void UpdateSystemPermissionCacheEntry(ContentSettingsType type,
                                        SystemPermission result) {
    if (type == ContentSettingsType::MEDIASTREAM_CAMERA ||
        type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
      camera_status_ = result;
    } else if (type == ContentSettingsType::MEDIASTREAM_MIC) {
      mic_status_ = result;
    }
  }

  // This function is static because Chromium prevents binding non-void member
  // functions to WeakPtrs. Making it static allows passing the WeakPtr as a
  // normal parameter and returning a value that can be chained using `.Then()`.
  static bool ProcessDeniedPermissionResult(
      base::WeakPtr<PlatformHandleImpl> platform_handle,
      ContentSettingsType type,
      SystemPermission result) {
    if (platform_handle) {
      platform_handle->UpdateSystemPermissionCacheEntry(type, result);
    }
    return IsSystemPermissionDenied(result);
  }

  void OnSystemPermissionRequestFinished(
      ContentSettingsType type,
      SystemPermissionResponseCallback callback) {
    // Since a permission request has likely updated the permission statuses,
    // update the cache first, and only then call the callback since it's
    // expected the calling code will want to check the permission status
    // immediately when the callback runs.
    RefreshSystemPermissionSettings(std::move(callback));
  }

  SystemPermission GetCachedSystemPermission(ContentSettingsType type) {
    if (type == ContentSettingsType::MEDIASTREAM_CAMERA ||
        type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
      return camera_status_;
    } else if (type == ContentSettingsType::MEDIASTREAM_MIC) {
      return mic_status_;
    }
    NOTREACHED();
  }

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

  SystemPermission camera_status_ = SystemPermission::kNotDetermined;
  SystemPermission mic_status_ = SystemPermission::kNotDetermined;

  std::vector<SystemPermissionResponseCallback> geolocation_callbacks_;
  std::unique_ptr<ScopedObservation> observation_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::WeakPtrFactory<PlatformHandleImpl> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<PlatformHandle> PlatformHandle::Create() {
  return std::make_unique<PlatformHandleImpl>();
}
}  // namespace system_permission_settings
