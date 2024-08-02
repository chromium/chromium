// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <memory>

#include "base/check_deref.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "chrome/browser/permissions/system/geolocation_observation.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

static_assert(BUILDFLAG(IS_WIN));

namespace system_permission_settings {

namespace {

class PlatformHandleImpl : public PlatformHandle {
 public:
  // PlatformHandle:
  bool CanPrompt(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION: {
        if (base::FeatureList::IsEnabled(
                features::kWinSystemLocationPermission)) {
          return device::GeolocationSystemPermissionManager::GetInstance()
                     ->GetSystemPermission() ==
                 device::LocationSystemPermissionStatus::kNotDetermined;
        } else {
          return false;
        }
      }
      default:
        return false;
    }
  }

  bool IsDenied(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION:
        if (base::FeatureList::IsEnabled(
                features::kWinSystemLocationPermission)) {
          return device::GeolocationSystemPermissionManager::GetInstance()
                     ->GetSystemPermission() ==
                 device::LocationSystemPermissionStatus::kDenied;
        } else {
          return false;
        }
      default:
        return false;
    }
  }

  bool IsAllowed(ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION:
        if (base::FeatureList::IsEnabled(
                features::kWinSystemLocationPermission)) {
          return device::GeolocationSystemPermissionManager::GetInstance()
                     ->GetSystemPermission() ==
                 device::LocationSystemPermissionStatus::kAllowed;
        } else {
          return true;
        }
      default:
        return true;
    }
  }

  void OpenSystemSettings(content::WebContents* web_contents,
                          ContentSettingsType type) override {
    switch (type) {
      case ContentSettingsType::GEOLOCATION: {
        if (base::FeatureList::IsEnabled(
                features::kWinSystemLocationPermission)) {
          device::GeolocationSystemPermissionManager::GetInstance()
              ->OpenSystemPermissionSetting();
        }
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
        if (!base::FeatureList::IsEnabled(
                features::kWinSystemLocationPermission)) {
          return;
        }
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
