// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <memory>

#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

static_assert(BUILDFLAG(IS_WIN));

class SystemPermissionSettingsWin
    : public SystemPermissionSettings,
      public device::GeolocationSystemPermissionManager::PermissionObserver {
 public:
  SystemPermissionSettingsWin() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Initialize system permission settings for geolocation based on the
    // feature flag.
    if (base::FeatureList::IsEnabled(features::kWinSystemLocationPermission)) {
      // If the feature is enabled, retrieve and observe the system permission
      // status.
      auto* geolocation_system_permission_manager =
          device::GeolocationSystemPermissionManager::GetInstance();
      CHECK(geolocation_system_permission_manager);
      observation_.Observe(geolocation_system_permission_manager);
    }
  }

  ~SystemPermissionSettingsWin() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    FlushGeolocationCallbacks();
  }

  bool CanPrompt(ContentSettingsType type) const override {
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

  bool IsDeniedImpl(ContentSettingsType type) const override {
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

  bool IsAllowedImpl(ContentSettingsType type) const override {
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
                          ContentSettingsType type) const override {
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
        geolocation_callbacks_.push_back(std::move(callback));
        // The system permission prompt is modal and requires a user decision
        // (Allow or Deny) before it can be dismissed.
        if (geolocation_callbacks_.size() == 1u &&
            base::FeatureList::IsEnabled(
                features::kWinSystemLocationPermission)) {
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

// static
std::unique_ptr<SystemPermissionSettings>
SystemPermissionSettings::CreateImpl() {
  return std::make_unique<SystemPermissionSettingsWin>();
}
