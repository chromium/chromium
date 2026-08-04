// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/browser/permissions/system/geolocation_observation.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/permissions/system/system_media_permission_cache.h"
#include "chrome/browser/permissions/system/system_media_source_win.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

static_assert(BUILDFLAG(IS_WIN));

namespace system_permission_settings {

namespace {

SystemPermission CheckVideoCapturePermission() {
  base::win::ScopedCOMInitializer com_initializer;
  switch (SystemMediaSourceWin::GetInstance().SystemPermissionStatus(
      ContentSettingsType::MEDIASTREAM_CAMERA)) {
    case SystemMediaSourceWin::Status::kNotDetermined:
      return SystemPermission::kNotDetermined;
    case SystemMediaSourceWin::Status::kDenied:
      return SystemPermission::kDenied;
    case SystemMediaSourceWin::Status::kAllowed:
      return SystemPermission::kAllowed;
  }
}

SystemPermission CheckAudioCapturePermission() {
  base::win::ScopedCOMInitializer com_initializer;
  switch (SystemMediaSourceWin::GetInstance().SystemPermissionStatus(
      ContentSettingsType::MEDIASTREAM_MIC)) {
    case SystemMediaSourceWin::Status::kNotDetermined:
      return SystemPermission::kNotDetermined;
    case SystemMediaSourceWin::Status::kDenied:
      return SystemPermission::kDenied;
    case SystemMediaSourceWin::Status::kAllowed:
      return SystemPermission::kAllowed;
  }
}

class PlatformHandleImpl : public PlatformHandle {
 public:
  PlatformHandleImpl()
      : media_cache_(
            base::BindOnce([](const base::TaskTraits& traits)
                               -> scoped_refptr<base::SequencedTaskRunner> {
              return base::ThreadPool::CreateCOMSTATaskRunner(traits);
            }),
            base::BindRepeating(&CheckVideoCapturePermission),
            base::BindRepeating(&CheckAudioCapturePermission)) {}

  PlatformHandleImpl(const PlatformHandleImpl&) = delete;
  PlatformHandleImpl& operator=(const PlatformHandleImpl&) = delete;

  ~PlatformHandleImpl() override = default;

  // PlatformHandle:
  bool CanPrompt(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kNotDetermined;
      // crbug.com/414523295: while the status of camera/microphone can be
      // determined, we currently don't support requesting them on Windows.
      // Until this is fixed we will return `false`.
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        return false;
      default:
        return false;
    }
  }

  bool IsDenied(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kDenied;
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        return media_cache_.IsDenied(type);
      default:
        return false;
    }
  }

  bool IsAllowed(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION:
        return device::GeolocationSystemPermissionManager::GetInstance()
                   ->GetSystemPermission() ==
               device::LocationSystemPermissionStatus::kAllowed;
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        return media_cache_.IsAllowed(type);
      default:
        return true;
    }
  }

  void IsDeniedFresh(ContentSettingsType type,
                     SystemPermissionDeniedCallback callback) override {
    if (type == ContentSettingsType::MEDIASTREAM_MIC ||
        type == ContentSettingsType::MEDIASTREAM_CAMERA ||
        type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
      media_cache_.IsDeniedFresh(type, std::move(callback));
      return;
    }
    std::move(callback).Run(IsDenied(type));
  }

  void OpenSystemSettings(content::WebContents* web_contents,
                          ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION: {
        device::GeolocationSystemPermissionManager::GetInstance()
            ->OpenSystemPermissionSetting();
        return;
      }
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM: {
        SystemMediaSourceWin::GetInstance().OpenSystemPermissionSetting(type);
        return;
      }
      default:
        NOTREACHED();
    }
  }

  void Request(ContentSettingsType type,
               SystemPermissionResponseCallback callback) override {
    switch (type) {
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
