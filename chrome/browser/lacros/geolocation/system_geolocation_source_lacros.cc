// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/geolocation/system_geolocation_source_lacros.h"

#include "ash/constants/geolocation_access_level.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/grit/branded_strings.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "chromeos/lacros/lacros_service.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "ui/base/l10n/l10n_util.h"

SystemGeolocationSourceLacros::SystemGeolocationSourceLacros()
    : permission_update_callback_(base::DoNothing()) {
  // Binding to remote for pref observation.
  crosapi_pref_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kUserGeolocationAccessLevel,
      base::BindRepeating(&SystemGeolocationSourceLacros::OnPrefChanged,
                          weak_factory_.GetWeakPtr()));
}

SystemGeolocationSourceLacros::~SystemGeolocationSourceLacros() = default;

// static
std::unique_ptr<device::GeolocationSystemPermissionManager>
SystemGeolocationSourceLacros::
    CreateGeolocationSystemPermissionManagerOnLacros() {
  return std::make_unique<device::GeolocationSystemPermissionManager>(
      std::make_unique<SystemGeolocationSourceLacros>());
}

void SystemGeolocationSourceLacros::RegisterPermissionUpdateCallback(
    PermissionUpdateCallback callback) {
  permission_update_callback_ = std::move(callback);
  if (current_status_ ==
      device::LocationSystemPermissionStatus::kNotDetermined) {
    // This is here to support older versions of Ash that do not send the system
    // geolocation switch via crosapi.
    // The original behavior before the system wide switch was introduced was to
    // allow, so we keep this as the default behavior when the system doesn't
    // indicate othervise.
    // TODO(272426671): clean this up when we can safely assume that Ash
    // provides the value.
    permission_update_callback_.Run(
        device::LocationSystemPermissionStatus::kAllowed);
    return;
  }
  // If available, pass the (up-to-date) status into the new callback
  permission_update_callback_.Run(current_status_);
}

void SystemGeolocationSourceLacros::OpenSystemPermissionSetting() {
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
    service->OpenUrl(
        GURL("chrome://os-settings/osPrivacy/privacyHub/geolocation"));
  }
}

void SystemGeolocationSourceLacros::OnPrefChanged(base::Value value) {
  if (!value.is_int()) {
    LOG(ERROR) << "GeolocationSourceLacros received a non-integral value";
    return;
  }
  switch (static_cast<ash::GeolocationAccessLevel>(value.GetInt())) {
    case ash::GeolocationAccessLevel::kDisallowed:
      current_status_ = device::LocationSystemPermissionStatus::kDenied;
      break;
    case ash::GeolocationAccessLevel::kAllowed:
      current_status_ = device::LocationSystemPermissionStatus::kAllowed;
      break;
    case ash::GeolocationAccessLevel::kOnlyAllowedForSystem:
      current_status_ = device::LocationSystemPermissionStatus::kDenied;
      break;
    default:
      LOG(ERROR) << "Incorrect GeolocationAccessLevel: " << value.GetInt();
      return;
  }

  permission_update_callback_.Run(current_status_);
}
