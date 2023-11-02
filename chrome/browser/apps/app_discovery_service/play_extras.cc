// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/play_extras.h"

#include <memory>

namespace apps {

PlayExtras::PlayExtras(const std::string& package_name,
                       const GURL& icon_url,
                       const std::u16string& category,
                       const std::u16string& description,
                       const std::u16string& content_rating,
                       const GURL& content_rating_icon_url,
                       const bool has_in_app_purchases,
                       const bool was_previously_installed,
                       const bool contains_ads,
                       const bool optimized_for_chrome)
    : package_name_(package_name),
      icon_url_(icon_url),
      category_(category),
      description_(description),
      content_rating_(content_rating),
      content_rating_icon_url_(content_rating_icon_url),
      has_in_app_purchases_(has_in_app_purchases),
      was_previously_installed_(was_previously_installed),
      contains_ads_(contains_ads),
      optimized_for_chrome_(optimized_for_chrome) {}

PlayExtras::PlayExtras(const PlayExtras&) = default;

PlayExtras::~PlayExtras() = default;

std::unique_ptr<SourceExtras> PlayExtras::Clone() {
  return std::make_unique<PlayExtras>(*this);
}

PlayExtras* PlayExtras::AsPlayExtras() {
  return this;
}

const std::string& PlayExtras::GetPackageName() const {
  return package_name_;
}

const GURL& PlayExtras::GetIconUrl() const {
  return icon_url_;
}

const std::u16string& PlayExtras::GetCategory() const {
  return category_;
}

const std::u16string& PlayExtras::GetDescription() const {
  return description_;
}

const std::u16string& PlayExtras::GetContentRating() const {
  return content_rating_;
}

const GURL& PlayExtras::GetContentRatingIconUrl() const {
  return content_rating_icon_url_;
}

bool PlayExtras::GetHasInAppPurchases() const {
  return has_in_app_purchases_;
}

bool PlayExtras::GetWasPreviouslyInstalled() const {
  return was_previously_installed_;
}

bool PlayExtras::GetContainsAds() const {
  return contains_ads_;
}

bool PlayExtras::GetOptimizedForChrome() const {
  return optimized_for_chrome_;
}

}  // namespace apps
