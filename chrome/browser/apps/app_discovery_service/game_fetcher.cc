// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_fetcher.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/country_codes/country_codes.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
extern const char kDefaultLocale[] = "en-US";

bool AvailableInCurrentLocale(
    const apps::proto::LocaleAvailability& app_with_locale) {
  int current_country_id = country_codes::GetCurrentCountryID();
  // TODO(melzhang) : This should not be returning -1. Return true for now so
  // that we have data during development.
  if (current_country_id == -1) {
    return true;
  }
  for (const auto& country_code : app_with_locale.available_country_codes()) {
    int country_id = country_codes::CountryStringToCountryID(country_code);
    if (country_id == current_country_id) {
      return true;
    }
  }
  return false;
}

std::u16string GetLocalisedName(
    const apps::proto::LocaleAvailability& app_with_locale,
    Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);
  std::string locale = prefs->GetString(language::prefs::kApplicationLocale);
  std::string resolved_locale;
  l10n_util::CheckAndResolveLocale(locale, &resolved_locale,
                                   /*perform_io=*/false);
  std::string localised_name;
  std::string fallback_name;
  for (const auto& available_localised_name :
       app_with_locale.available_localised_names()) {
    // When the current locale does not match any of the supported
    // |language_codes|, we default to en-US. Store the en-US name as a
    // fallback.
    if (available_localised_name.language_code() == kDefaultLocale) {
      fallback_name = available_localised_name.name_in_language();
    }
    if (available_localised_name.language_code() != resolved_locale) {
      continue;
    }
    localised_name = available_localised_name.name_in_language();
  }
  if (localised_name.empty()) {
    DCHECK(!fallback_name.empty());
    localised_name = fallback_name;
  }
  return base::UTF8ToUTF16(localised_name);
}

}  // namespace

namespace apps {

GameFetcher::GameFetcher(Profile* profile) : profile_(profile) {
  app_provisioning_data_observeration_.Observe(
      AppProvisioningDataManager::Get());
}

GameFetcher::~GameFetcher() = default;

void GameFetcher::GetApps(ResultCallback callback) {
  auto error = last_results_.empty() ? DiscoveryError::kErrorRequestFailed
                                     : DiscoveryError::kSuccess;
  std::move(callback).Run(last_results_, error);
}

base::CallbackListSubscription GameFetcher::RegisterForAppUpdates(
    RepeatingResultCallback callback) {
  return result_callback_list_.Add(std::move(callback));
}

void GameFetcher::OnAppDataUpdated(const proto::AppWithLocaleList& app_data) {
  last_results_ = GetAppsForCurrentLocale(app_data);
  result_callback_list_.Notify(last_results_);
}

std::vector<Result> GameFetcher::GetAppsForCurrentLocale(
    const proto::AppWithLocaleList& app_data) {
  std::vector<Result> results;
  for (const auto& app_with_locale : app_data.app_with_locale()) {
    if (!AvailableInCurrentLocale(app_with_locale.locale_availability())) {
      continue;
    }
    auto extras = std::make_unique<GameExtras>(
        absl::nullopt, GameExtras::Source::kTestSource, GURL());
    results.push_back(Result(
        AppSource::kGames, app_with_locale.app().app_id_for_platform(),
        GetLocalisedName(app_with_locale.locale_availability(), profile_),
        std::move(extras)));
  }
  return results;
}

}  // namespace apps
