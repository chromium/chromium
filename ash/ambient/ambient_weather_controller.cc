// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_weather_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

// TODO(jamescook): Rename to "ambient weather".
constexpr net::NetworkTrafficAnnotationTag kAmbientPhotoControllerTag =
    net::DefineNetworkTrafficAnnotation("ambient_photo_controller", R"(
        semantics {
          sender: "Ambient photo"
          description:
            "Download ambient image weather icon from Google."
          trigger:
            "Triggered periodically when the battery is charged and the user "
            "is idle."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is off by default and can be overridden by user."
         policy_exception_justification:
           "This feature is set by user settings.ambient_mode.enabled pref. "
           "The user setting is per device and cannot be overriden by admin."
        })");

void DownloadImageFromUrl(const std::string& url,
                          ImageDownloader::DownloadCallback callback) {
  DCHECK(!url.empty());

  // During shutdown, we may not have `ImageDownloader` when reach here.
  if (!ImageDownloader::Get()) {
    return;
  }

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  DCHECK(active_user_session);

  ImageDownloader::Get()->Download(GURL(url), kAmbientPhotoControllerTag,
                                   active_user_session->user_info.account_id,
                                   std::move(callback));
}

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace

AmbientWeatherController::ScopedRefresher::ScopedRefresher(
    AmbientWeatherController* controller)
    : controller_(controller) {
  CHECK(controller_);
}

AmbientWeatherController::ScopedRefresher::~ScopedRefresher() {
  controller_->OnScopedRefresherDestroyed();
}

AmbientWeatherController::AmbientWeatherController(
    SimpleGeolocationProvider* const location_permission_provider)
    : location_permission_provider_(location_permission_provider),
      weather_model_(std::make_unique<AmbientWeatherModel>()) {
  CHECK_NE(location_permission_provider_, nullptr);
  location_permission_provider_->AddObserver(this);
}

AmbientWeatherController::~AmbientWeatherController() {
  CHECK_NE(location_permission_provider_, nullptr);
  location_permission_provider_->RemoveObserver(this);
}

void AmbientWeatherController::OnGeolocationPermissionChanged(bool enabled) {
  OnPermissionChanged();
}

void AmbientWeatherController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  pref_change_registrar_.Reset();
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kContextualGoogleIntegrationsConfiguration,
      base::BindRepeating(
          &AmbientWeatherController::OnWeatherIntegrationPreferenceChanged,
          base::Unretained(this)));
}

std::unique_ptr<AmbientWeatherController::ScopedRefresher>
AmbientWeatherController::CreateScopedRefresher() {
  ++num_active_scoped_refreshers_;
  if (!weather_refresh_timer_.IsRunning() && IsGeolocationUsageAllowed() &&
      !IsWeatherDisabledByPolicy()) {
    FetchWeather();
    weather_refresh_timer_.Start(FROM_HERE, kWeatherRefreshInterval, this,
                                 &AmbientWeatherController::FetchWeather);
  }
  // `WrapUnique()` needed for ScopedRefresher's private constructor.
  return base::WrapUnique(new ScopedRefresher(this));
}

void AmbientWeatherController::FetchWeather() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchWeather(
          /*weather_client_id=*/std::nullopt,
          base::BindOnce(
              &AmbientWeatherController::StartDownloadingWeatherConditionIcon,
              weak_factory_.GetWeakPtr()));
}

void AmbientWeatherController::StartDownloadingWeatherConditionIcon(
    const std::optional<WeatherInfo>& weather_info) {
  if (!weather_info) {
    LOG(WARNING) << "No weather info included in the response.";
    return;
  }

  if (!weather_info->temp_f.has_value()) {
    LOG(WARNING) << "No temperature included in weather info.";
    return;
  }

  if (weather_info->condition_icon_url.value_or(std::string()).empty()) {
    LOG(WARNING) << "No value found for condition icon url in the weather info "
                    "response.";
    return;
  }

  // Ideally we should avoid downloading from the same url again to reduce the
  // overhead, as it's unlikely that the weather condition is changing
  // frequently during the day.
  // TODO(meilinw): avoid repeated downloading by caching the last N url hashes,
  // where N should depend on the icon image size.
  DownloadImageFromUrl(
      weather_info->condition_icon_url.value(),
      base::BindOnce(
          &AmbientWeatherController::OnWeatherConditionIconDownloaded,
          weak_factory_.GetWeakPtr(), weather_info->temp_f.value(),
          weather_info->show_celsius));
}

void AmbientWeatherController::OnWeatherConditionIconDownloaded(
    float temp_f,
    bool show_celsius,
    const gfx::ImageSkia& icon) {
  // For now we only show the weather card when both fields have values.
  // TODO(meilinw): optimize the behavior with more specific error handling.
  if (icon.isNull())
    return;

  weather_model_->UpdateWeatherInfo(icon, temp_f, show_celsius);
}

bool AmbientWeatherController::IsGeolocationUsageAllowed() {
  return location_permission_provider_->IsGeolocationUsageAllowedForSystem();
}

bool AmbientWeatherController::IsWeatherDisabledByPolicy() {
  const auto* pref_service = GetPrefService();
  return !pref_service ||
         !base::Contains(pref_service->GetList(
                             prefs::kContextualGoogleIntegrationsConfiguration),
                         prefs::kWeatherIntegrationName);
}

void AmbientWeatherController::ClearAmbientWeatherModel() {
  weather_model_->UpdateWeatherInfo(gfx::ImageSkia(), 0.0f, true);
}

void AmbientWeatherController::OnScopedRefresherDestroyed() {
  --num_active_scoped_refreshers_;
  CHECK_GE(num_active_scoped_refreshers_, 0);
  if (num_active_scoped_refreshers_ == 0) {
    // This may not have user-visible effects, but refreshing the weather when
    // there's no UI using it is wasting network/server resources.
    weather_refresh_timer_.Stop();
  }
}

void AmbientWeatherController::OnWeatherIntegrationPreferenceChanged(
    const std::string& pref_name) {
  OnPermissionChanged();
}

void AmbientWeatherController::OnPermissionChanged() {
  // When system permission is blocked, stop scheduling new requests and drop
  // all pending requests. Also clears the weather model cache for privacy
  // reasons.
  if (!IsGeolocationUsageAllowed() || IsWeatherDisabledByPolicy()) {
    weather_refresh_timer_.Stop();
    weak_factory_.InvalidateWeakPtrs();
    ClearAmbientWeatherModel();
    return;
  }

  // System permission is granted, resume scheduler if needed.
  if (num_active_scoped_refreshers_ > 0) {
    FetchWeather();
    weather_refresh_timer_.Start(FROM_HERE, kWeatherRefreshInterval, this,
                                 &AmbientWeatherController::FetchWeather);
  }
}

}  // namespace ash
