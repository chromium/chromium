// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_weather_provider.h"

#include <string>

#include "ash/ambient/ambient_controller.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/birch_ranker.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/weather_info.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {

BirchWeatherProvider::BirchWeatherProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {}

BirchWeatherProvider::~BirchWeatherProvider() = default;

void BirchWeatherProvider::RequestBirchDataFetch() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kDisableBirchWeatherApiForTesting) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kEnableBirchWeatherApiForTestingOverride)) {
    // Avoid calling into the Weather API when the switch is set for testing.
    Shell::Get()->birch_model()->SetWeatherItems({});
    return;
  }
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
  const UserSession* session =
      Shell::Get()->session_controller()->GetUserSession(0);
  if (session->user_info.account_id == user_manager::StubAccountId()) {
    // Weather is not allowed for stub users, which don't have valid Gaia IDs.
    birch_model_->SetWeatherItems({});
    return;
  }
  // The ranker only shows weather in the mornings, so only fetch the data in
  // the mornings to limit QPS on the backend.
  BirchRanker ranker(base::Time::Now());
  if (!ranker.IsMorning()) {
    birch_model_->SetWeatherItems({});
    return;
  }

  // Use the cache if it has data and the last fetch was recent.
  if (last_weather_info_.has_value() &&
      base::Time::Now() < last_fetch_time_ + base::Minutes(5)) {
    OnWeatherInfoFetched(last_weather_info_);
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
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchWeather("chromeos-system-ui",
                     base::BindOnce(&BirchWeatherProvider::OnWeatherInfoFetched,
                                    weak_factory_.GetWeakPtr()));
}

void BirchWeatherProvider::OnWeatherInfoFetched(
    const std::optional<WeatherInfo>& weather_info) {
  is_fetching_ = false;

  // Check for partial data.
  if (!weather_info || !weather_info->temp_f.has_value() ||
      !weather_info->condition_icon_url ||
      !weather_info->condition_description ||
      weather_info->condition_icon_url->empty()) {
    last_weather_info_.reset();
    birch_model_->SetWeatherItems({});
    return;
  }

  // Cache for future requests.
  last_weather_info_ = weather_info;
  last_fetch_time_ = base::Time::Now();

  // Add the item to the model. Note that we ignore "show_celsius" in favor
  // of a client-side pref for temperature units.
  AddItemToBirchModel(base::UTF8ToUTF16(*weather_info->condition_description),
                      *weather_info->temp_f, *weather_info->condition_icon_url);
}

void BirchWeatherProvider::AddItemToBirchModel(
    const std::u16string& weather_description,
    float temp_f,
    const std::string& icon_url) {
  std::vector<BirchWeatherItem> items;
  items.emplace_back(weather_description, temp_f, GURL(icon_url));
  birch_model_->SetWeatherItems(std::move(items));
}

void BirchWeatherProvider::ResetCacheForTest() {
  last_weather_info_.reset();
  last_fetch_time_ = base::Time();
}

}  // namespace ash
