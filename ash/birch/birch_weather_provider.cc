// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_weather_provider.h"

#include <string>

#include "ash/ambient/ambient_controller.h"
#include "ash/birch/birch_icon_cache.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag kWeatherIconTag =
    net::DefineNetworkTrafficAnnotation("weather_icon", R"(
        semantics {
          sender: "Post-login glanceables"
          description:
            "Download weather icon image from Google. The icon is used for "
            "suggestion chip buttons for activities the user might want to "
            "perform after login or from overview mode (e.g. view the weather)."
          trigger:
            "User logs in to the device or enters overview mode."
          data: "None."
          user_data: {
            type: NONE
          }
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chromeos-launcher@google.com"
            }
          }
          last_reviewed: "2024-05-30"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be enabled/disabled by the user in the "
            "suggestion chip button context menu."
          chrome_policy {
            ContextualGoogleIntegrationsEnabled {
              ContextualGoogleIntegrationsEnabled: false
            }
          }
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
  const auto* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!pref_service ||
      !base::Contains(pref_service->GetList(
                          prefs::kContextualGoogleIntegrationsConfiguration),
                      prefs::kWeatherIntegrationName)) {
    // Weather integration is disabled by policy.
    Shell::Get()->birch_model()->SetWeatherItems({});
    return;
  }
  if (!SimpleGeolocationProvider::GetInstance()
           ->IsGeolocationUsageAllowedForSystem()) {
    // Weather is not allowed if geolocation is off.
    birch_model_->SetWeatherItems({});
    return;
  }
  // Only allow one fetch at a time.
  if (is_fetching_) {
    return;
  }
  is_fetching_ = true;

  if (!birch_model_->birch_client()) {
    // BirchClient may be null in tests.
    FetchWeather();
    return;
  }
  // Fetching weather requires auth, but early in startup refresh tokens may not
  // be loaded yet. Ensure refresh tokens are loaded before doing the fetch.
  birch_model_->birch_client()->WaitForRefreshTokens(base::BindOnce(
      &BirchWeatherProvider::FetchWeather, weak_factory_.GetWeakPtr()));
}

void BirchWeatherProvider::FetchWeather() {
  const bool prefer_prod_endpoint = base::GetFieldTrialParamByFeatureAsBool(
      features::kBirchWeather, "prod_weather_endpoint", false);
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchWeather("chromeos-system-ui",
                     /*prefer_alpha_endpoint=*/!prefer_prod_endpoint,
                     base::BindOnce(&BirchWeatherProvider::OnWeatherInfoFetched,
                                    weak_factory_.GetWeakPtr()));
}

void BirchWeatherProvider::OnWeatherInfoFetched(
    const std::optional<WeatherInfo>& weather_info) {
  is_fetching_ = false;
  if (!weather_info || !weather_info->temp_f.has_value() ||
      !weather_info->condition_icon_url ||
      !weather_info->condition_description ||
      weather_info->condition_icon_url->empty()) {
    birch_model_->SetWeatherItems({});
    return;
  }

  // Check for a cached icon.
  gfx::ImageSkia icon = Shell::Get()->birch_model()->icon_cache()->Get(
      *weather_info->condition_icon_url);
  if (!icon.isNull()) {
    // Use the cached icon.
    AddItemToBirchModel(base::UTF8ToUTF16(*weather_info->condition_description),
                        *weather_info->temp_f, icon);
    return;
  }

  // Download the weather condition icon. Note that we ignore "show_celsius" in
  // favor of a client-side pref.
  DownloadImageFromUrl(
      *weather_info->condition_icon_url,
      base::BindOnce(&BirchWeatherProvider::OnWeatherConditionIconDownloaded,
                     weak_factory_.GetWeakPtr(),
                     *weather_info->condition_icon_url,
                     base::UTF8ToUTF16(*weather_info->condition_description),
                     *weather_info->temp_f));
}

void BirchWeatherProvider::OnWeatherConditionIconDownloaded(
    const std::string& condition_icon_url,
    const std::u16string& weather_description,
    float temp_f,
    const gfx::ImageSkia& icon) {
  if (icon.isNull()) {
    birch_model_->SetWeatherItems({});
    return;
  }

  // Add the icon to the cache.
  Shell::Get()->birch_model()->icon_cache()->Put(condition_icon_url, icon);

  AddItemToBirchModel(weather_description, temp_f, icon);
}

void BirchWeatherProvider::AddItemToBirchModel(
    const std::u16string& weather_description,
    float temp_f,
    const gfx::ImageSkia& icon) {
  std::vector<BirchWeatherItem> items;
  items.emplace_back(weather_description, temp_f,
                     ui::ImageModel::FromImageSkia(icon));
  birch_model_->SetWeatherItems(std::move(items));
}

}  // namespace ash
