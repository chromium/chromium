// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_fetcher.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/timezone.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/country_codes/country_codes.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace {
extern const char kDefaultLocale[] = "en-US";

bool AvailableInCurrentTimezoneLocale(
    const apps::proto::LocaleAvailability& app_with_locale) {
  auto local_country_code = base::CountryCodeForCurrentTimezone();
  if (local_country_code.empty()) {
    return false;
  }
  for (const auto& country_code : app_with_locale.available_country_codes()) {
    if (country_code == local_country_code) {
      return true;
    }
  }
  return false;
}

bool AvailableInCurrentLocale(
    const apps::proto::LocaleAvailability& app_with_locale) {
  int current_country_id = country_codes::GetCurrentCountryID();
  if (current_country_id == -1) {
    // Try using the timezone to get the country as a fallback.
    return AvailableInCurrentTimezoneLocale(app_with_locale);
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

absl::optional<std::vector<std::u16string>> GetPlatforms(
    const apps::proto::App& app) {
  if (app.available_stores_size() == 0) {
    return absl::nullopt;
  }

  std::vector<std::u16string> store_names;
  for (const auto& store : app.available_stores()) {
    if (!store.store_label().empty()) {
      store_names.push_back(base::UTF8ToUTF16(store.store_label()));
    }
  }
  return store_names;
}

std::string ReadFileToString(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    result.clear();
  }
  return result;
}

void ReplyWithIcon(apps::GetIconCallback callback,
                   const SkBitmap& decoded_image) {
  if (decoded_image.empty()) {
    std::move(callback).Run(gfx::ImageSkia(),
                            apps::DiscoveryError::kErrorMalformedData);
    return;
  }

  std::move(callback).Run(gfx::ImageSkia::CreateFrom1xBitmap(decoded_image),
                          apps::DiscoveryError::kSuccess);
}

void DecodeIcon(apps::GetIconCallback callback,
                int32_t size_hint_in_dip,
                const std::string& icon_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (icon_data.empty()) {
    std::move(callback).Run(gfx::ImageSkia(),
                            apps::DiscoveryError::kErrorRequestFailed);
    return;
  }

  data_decoder::DecodeImageIsolated(
      base::as_bytes(base::make_span(icon_data)),
      data_decoder::mojom::ImageCodec::kDefault, true,
      data_decoder::kDefaultMaxSizeInBytes,
      gfx::Size(size_hint_in_dip, size_hint_in_dip),
      base::BindOnce(&ReplyWithIcon, std::move(callback)));
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

void GameFetcher::GetIcon(const std::string& app_id,
                          int32_t size_hint_in_dip,
                          GetIconCallback callback) {
  if (last_results_.empty()) {
    std::move(callback).Run(gfx::ImageSkia(),
                            DiscoveryError::kErrorRequestFailed);
    return;
  }

  Result* app = nullptr;

  for (Result& candidate : last_results_) {
    if (candidate.GetAppId() == app_id) {
      app = &candidate;
    }
  }

  if (!app) {
    std::move(callback).Run(gfx::ImageSkia(),
                            DiscoveryError::kErrorRequestFailed);
    return;
  }

  base::FilePath icon_path =
      AppProvisioningDataManager::Get()->GetDataFilePath().Append(
          app->GetSourceExtras()->AsGameExtras()->GetRelativeIconPath());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToString, icon_path),
      base::BindOnce(&DecodeIcon, std::move(callback), size_hint_in_dip));
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
        GetPlatforms(app_with_locale.app()),
        base::UTF8ToUTF16(app_with_locale.app().source_name()),
        base::UTF8ToUTF16(app_with_locale.app().publisher_name()),
        base::FilePath(app_with_locale.app().icon_info().icon_path()),
        app_with_locale.app().icon_info().is_masking_allowed(),
        GURL(app_with_locale.app().deeplink()));
    results.push_back(Result(
        AppSource::kGames, app_with_locale.app().app_id_for_platform(),
        GetLocalisedName(app_with_locale.locale_availability(), profile_),
        std::move(extras)));
  }
  return results;
}

}  // namespace apps
