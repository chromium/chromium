// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_weather_provider.h"

#include <string>

#include "ash/ambient/ambient_controller.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag kWeatherIconTag =
    net::DefineNetworkTrafficAnnotation("weather_icon", R"(
        semantics {
          sender: "Birch feature"
          description:
            "Download weather icon image from Google."
          trigger:
            "The user opens an UI surface associated with birch feature."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is off by default."
         policy_exception_justification:
           "Policy is planned, but not yet implemented."
        })");

void DownloadImageFromUrl(const std::string& url_string,
                          ImageDownloader::DownloadCallback callback) {
  GURL url(url_string);
  if (!url.is_valid()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  DCHECK(active_user_session);

  ImageDownloader::Get()->Download(url, kWeatherIconTag,
                                   active_user_session->user_info.account_id,
                                   std::move(callback));
}

}  // namespace

BirchWeatherProvider::BirchWeatherProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {}

BirchWeatherProvider::~BirchWeatherProvider() = default;

void BirchWeatherProvider::RequestBirchDataFetch() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchWeather(base::BindOnce(&BirchWeatherProvider::OnWeatherInfoFetched,
                                    weak_factory_.GetWeakPtr()));
}

void BirchWeatherProvider::OnWeatherInfoFetched(
    const std::optional<WeatherInfo>& weather_info) {
  if (!weather_info || !weather_info->temp_f.has_value() ||
      !weather_info->condition_icon_url ||
      !weather_info->condition_description ||
      weather_info->condition_icon_url->empty()) {
    birch_model_->SetWeatherItems({});
    return;
  }

  // Ideally we should avoid downloading from the same url again to reduce the
  // overhead, as it's unlikely that the weather condition is changing
  // frequently during the day.
  DownloadImageFromUrl(
      *weather_info->condition_icon_url,
      base::BindOnce(&BirchWeatherProvider::OnWeatherConditionIconDownloaded,
                     weak_factory_.GetWeakPtr(),
                     base::UTF8ToUTF16(*weather_info->condition_description),
                     *weather_info->temp_f, weather_info->show_celsius));
}

void BirchWeatherProvider::OnWeatherConditionIconDownloaded(
    const std::u16string& weather_description,
    float temp_f,
    bool show_celsius,
    const gfx::ImageSkia& icon) {
  if (icon.isNull()) {
    birch_model_->SetWeatherItems({});
    return;
  }

  std::u16string temperature_string =
      show_celsius ? l10n_util::GetStringFUTF16Int(
                         IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_CELSIUS,
                         static_cast<int>((temp_f - 32) * 5 / 9))
                   : l10n_util::GetStringFUTF16Int(
                         IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_FAHRENHEIT,
                         static_cast<int>(temp_f));

  std::vector<BirchWeatherItem> items;
  items.emplace_back(weather_description, temperature_string,
                     ui::ImageModel::FromImageSkia(icon));
  birch_model_->SetWeatherItems(std::move(items));
}

}  // namespace ash
