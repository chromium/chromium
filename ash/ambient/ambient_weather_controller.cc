// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_weather_controller.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/account_id/account_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

}  // namespace

AmbientWeatherController::AmbientWeatherController()
    : weather_model_(std::make_unique<AmbientWeatherModel>()) {}

AmbientWeatherController::~AmbientWeatherController() = default;

void AmbientWeatherController::FetchWeather() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchWeather(base::BindOnce(
          &AmbientWeatherController::StartDownloadingWeatherConditionIcon,
          weak_factory_.GetWeakPtr()));
}

void AmbientWeatherController::StartDownloadingWeatherConditionIcon(
    const absl::optional<WeatherInfo>& weather_info) {
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

}  // namespace ash
