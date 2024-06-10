// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/geolocation_access_level.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"

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
  SystemPermissionSettingsImpl()
      : value_(static_cast<int>(ash::GeolocationAccessLevel::kAllowed)) {
    if (base::FeatureList::IsEnabled(
            content_settings::features::
                kCrosSystemLevelPermissionBlockedWarnings)) {
      auto* lacros_service = chromeos::LacrosService::Get();
      CHECK(lacros_service);

      if (!lacros_service->IsRegistered<crosapi::mojom::Prefs>()) {
        return;
      }
      if (!lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
        return;
      }
      mojo::Remote<crosapi::mojom::Prefs>& service =
          lacros_service->GetRemote<crosapi::mojom::Prefs>();
      if (!service.is_connected()) {
        return;
      }
      service->GetPref(
          crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
          base::BindOnce(&SystemPermissionSettingsImpl::UpdateValue,
                         base::Unretained(this)));
    }
  }

  bool IsPermissionDeniedImpl(ContentSettingsType type) const override {
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:  // fallthrough
      case ContentSettingsType::MEDIASTREAM_MIC: {
        if (!value_.is_bool()) {
          return false;
        }
        return !value_.GetBool();
      }
      case ContentSettingsType::GEOLOCATION: {
        if (!value_.is_int()) {
          return false;
        }
        switch (static_cast<ash::GeolocationAccessLevel>(value_.GetInt())) {
          case ash::GeolocationAccessLevel::kDisallowed:
            return true;
          case ash::GeolocationAccessLevel::kAllowed:
            return false;
          case ash::GeolocationAccessLevel::kOnlyAllowedForSystem:
            return true;
          default:
            LOG(ERROR) << "Incorrect GeolocationAccessLevel: "
                       << value_.GetInt();
            return false;
        }
      }
      default: {
        return false;
      }
    }
  }

  void UpdateValue(std::optional<base::Value> value) {
    if (value.has_value()) {
      value_ = std::move(*value);
    }
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

 private:
  base::Value value_;
  base::WeakPtrFactory<SystemPermissionSettingsImpl> weak_ptr_factory_{this};
};

std::unique_ptr<SystemPermissionSettings> SystemPermissionSettings::Create() {
  return std::make_unique<SystemPermissionSettingsImpl>();
}
