// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>

#include "ash/constants/geolocation_access_level.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chromeos/crosapi/mojom/prefs.mojom-shared.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS));

namespace {

void OpenURLInAsh(const std::string& url) {
  auto* lacros_service = chromeos::LacrosService::Get();
  CHECK(lacros_service);

  if (!lacros_service->IsRegistered<crosapi::mojom::UrlHandler>()) {
    return;
  }
  if (!lacros_service->IsAvailable<crosapi::mojom::UrlHandler>()) {
    return;
  }
  mojo::Remote<crosapi::mojom::UrlHandler>& service =
      lacros_service->GetRemote<crosapi::mojom::UrlHandler>();
  if (service.is_connected()) {
    // Open the appropriate CrOS system settings page.
    service->OpenUrl(GURL(url));
  }
}

}  // namespace

class SystemPermissionSettingsImpl : public SystemPermissionSettings {
 public:
  SystemPermissionSettingsImpl() {
    if (base::FeatureList::IsEnabled(
            content_settings::features::
                kCrosSystemLevelPermissionBlockedWarnings)) {
      camera_pref_observer_ = std::make_unique<CrosapiPrefObserver>(
          crosapi::mojom::PrefPath::kUserCameraAllowed,
          base::BindRepeating(&SystemPermissionSettingsImpl::OnPrefChanged,
                              base::Unretained(this),
                              ContentSettingsType::MEDIASTREAM_CAMERA));
      mic_pref_observer_ = std::make_unique<CrosapiPrefObserver>(
          crosapi::mojom::PrefPath::kUserMicrophoneAllowed,
          base::BindRepeating(&SystemPermissionSettingsImpl::OnPrefChanged,
                              base::Unretained(this),
                              ContentSettingsType::MEDIASTREAM_MIC));
      geolocation_pref_observer_ = std::make_unique<CrosapiPrefObserver>(
          crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
          base::BindRepeating(&SystemPermissionSettingsImpl::OnPrefChanged,
                              base::Unretained(this),
                              ContentSettingsType::GEOLOCATION));
    }
  }

  bool CanPrompt(ContentSettingsType type) const override { return false; }

  bool ConvertQueryResult(ContentSettingsType type, base::Value value) {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:  // fallthrough
      case ContentSettingsType::MEDIASTREAM_MIC: {
        CHECK(value.is_bool());
        return !value.GetBool();
      }
      case ContentSettingsType::GEOLOCATION: {
        CHECK(value.is_int());
        switch (static_cast<ash::GeolocationAccessLevel>(value.GetInt())) {
          case ash::GeolocationAccessLevel::kDisallowed:
            return true;
          case ash::GeolocationAccessLevel::kAllowed:
            return false;
          case ash::GeolocationAccessLevel::kOnlyAllowedForSystem:
            return true;
          default:
            LOG(ERROR) << "Incorrect GeolocationAccessLevel: "
                       << value.GetInt();
            return false;
        }
      }
      default: {
        LOG(ERROR) << "This ContentSettingsType is not controlled by the OS: "
                   << type;
        NOTREACHED();
      }
    }
  }

  bool IsAllowedImpl(ContentSettingsType type) const override {
    return !IsDeniedImpl(type);
  }

  void OnPrefChanged(ContentSettingsType type, base::Value value) {
    if (ConvertQueryResult(type, std::move(value))) {
      blocked_permissions_.insert(type);
    } else {
      blocked_permissions_.erase(type);
    }
  }

  bool IsDeniedImpl(ContentSettingsType type) const override {
    return blocked_permissions_.contains(type);
  }

  void OpenSystemSettings(content::WebContents*,
                          ContentSettingsType type) const override {
    if (base::FeatureList::IsEnabled(
            content_settings::features::
                kCrosSystemLevelPermissionBlockedWarnings)) {
      switch (type) {
        case ContentSettingsType::MEDIASTREAM_CAMERA: {
          OpenURLInAsh("chrome://os-settings/osPrivacy/privacyHub/camera");
          return;
        }
        case ContentSettingsType::MEDIASTREAM_MIC: {
          OpenURLInAsh("chrome://os-settings/osPrivacy/privacyHub/microphone");
          return;
        }
        case ContentSettingsType::GEOLOCATION: {
          OpenURLInAsh("chrome://os-settings/osPrivacy/privacyHub/geolocation");
          return;
        }
        default: {
          return;
        }
      }
    }
  }

  void Request(ContentSettingsType type,
               SystemPermissionResponseCallback callback) override {
    std::move(callback).Run();
    NOTREACHED();
  }

 private:
  std::unique_ptr<CrosapiPrefObserver> camera_pref_observer_;
  std::unique_ptr<CrosapiPrefObserver> mic_pref_observer_;
  std::unique_ptr<CrosapiPrefObserver> geolocation_pref_observer_;
  std::set<ContentSettingsType> blocked_permissions_;
  base::WeakPtrFactory<SystemPermissionSettingsImpl> weak_ptr_factory_{this};
};

std::unique_ptr<SystemPermissionSettings>
SystemPermissionSettings::CreateImpl() {
  return std::make_unique<SystemPermissionSettingsImpl>();
}
