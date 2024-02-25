// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/app_discovery_service/play_extras.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher.h"

namespace apps {

RecommendedArcAppFetcher::RecommendedArcAppFetcher(Profile* profile)
    : profile_(profile) {}
RecommendedArcAppFetcher::~RecommendedArcAppFetcher() = default;

void RecommendedArcAppFetcher::GetApps(ResultCallback callback) {
  // Only one request can ever be made at a time.
  DCHECK(!callback_);
  callback_ = std::move(callback);
  recommend_apps_fetcher_ = RecommendAppsFetcher::Create(profile_, this);
  recommend_apps_fetcher_->Start();
}

void RecommendedArcAppFetcher::OnLoadSuccess(base::Value app_list) {
  if (!callback_)
    return;
  if (!app_list.is_dict()) {
    std::move(callback_).Run({}, DiscoveryError::kErrorMalformedData);
    return;
  }

  const base::Value::List* apps = app_list.GetDict().FindList("recommendedApp");
  if (!apps || apps->empty()) {
    std::move(callback_).Run({}, DiscoveryError::kErrorMalformedData);
    return;
  }

  std::vector<Result> results;
  for (auto& big_app : *apps) {
    const base::Value::Dict* big_app_dict = big_app.GetIfDict();
    if (big_app_dict) {
      const base::Value::Dict* app = big_app_dict->FindDict("androidApp");
      if (!app) {
        continue;
      }
      const std::string* package_name = app->FindString("packageName");
      const std::string* title = app->FindString("title");
      if (!package_name || !title)
        continue;
      const std::string* icon_url =
          app->FindStringByDottedPath("icon.imageUri");
      const std::string* category = app->FindString("category");
      const std::string* app_description =
          app->FindStringByDottedPath("appDescription.shortDescription");
      const std::string* content_rating =
          app->FindStringByDottedPath("contentRating.name");
      const std::string* content_rating_url =
          app->FindStringByDottedPath("contentRating.image.imageUri");
      const std::string* in_app_purchases = app->FindStringByDottedPath(
          "inAppPurchaseInformation.disclaimerText");
      const std::string* previously_installed =
          app->FindStringByDottedPath("fastAppReinstall.explanationText");
      const std::string* contain_ads =
          app->FindStringByDottedPath("adsInformation.disclaimerText");
      const base::Value::Dict* optimized_for_chrome =
          big_app_dict->FindDict("merchCurated");

      auto extras = std::make_unique<PlayExtras>(
          *package_name, icon_url ? GURL(*icon_url) : GURL(),
          category ? base::UTF8ToUTF16(*category) : u"",
          app_description ? base::UTF8ToUTF16(*app_description) : u"",
          content_rating ? base::UTF8ToUTF16(*content_rating) : u"",
          content_rating_url ? GURL(*content_rating_url) : GURL(),
          (in_app_purchases != nullptr), (previously_installed != nullptr),
          (contain_ads != nullptr), (optimized_for_chrome != nullptr));
      results.emplace_back(Result(AppSource::kPlay, *package_name,
                                  base::UTF8ToUTF16(*title),
                                  std::move(extras)));
    }
  }
  std::move(callback_).Run(std::move(results), DiscoveryError::kSuccess);
  recommend_apps_fetcher_.reset();
}

void RecommendedArcAppFetcher::OnLoadError() {
  if (callback_) {
    std::move(callback_).Run({}, DiscoveryError::kErrorRequestFailed);
    recommend_apps_fetcher_.reset();
  }
}

void RecommendedArcAppFetcher::OnParseResponseError() {
  if (callback_) {
    std::move(callback_).Run({}, DiscoveryError::kErrorMalformedData);
    recommend_apps_fetcher_.reset();
  }
}

void RecommendedArcAppFetcher::SetCallbackForTesting(ResultCallback callback) {
  callback_ = std::move(callback);
}

}  // namespace apps
