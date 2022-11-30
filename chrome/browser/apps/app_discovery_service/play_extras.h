// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_PLAY_EXTRAS_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_PLAY_EXTRAS_H_

#include <string>

#include "chrome/browser/apps/app_discovery_service/result.h"
#include "url/gurl.h"

namespace apps {

class PlayExtras : public SourceExtras {
 public:
  PlayExtras(const std::string& package_name,
             const GURL& icon_url,
             const std::u16string& category,
             const std::u16string& description,
             const std::u16string& content_rating,
             const GURL& content_rating_icon_url,
             const bool in_app_purchases,
             const bool previously_installed,
             const bool contains_ads,
             const bool optimized_for_chrome);
  PlayExtras(const PlayExtras&);
  PlayExtras& operator=(const PlayExtras&) = delete;
  ~PlayExtras() override;

  // Result::SourceExtras:
  std::unique_ptr<SourceExtras> Clone() override;
  PlayExtras* AsPlayExtras() override;

  const std::string& GetPackageName() const;
  const GURL& GetIconUrl() const;
  const std::u16string& GetCategory() const;
  const std::u16string& GetDescription() const;
  const std::u16string& GetContentRating() const;
  const GURL& GetContentRatingIconUrl() const;
  bool GetHasInAppPurchases() const;
  // Whether or not this app was previously installed on a different device
  // that this user owns.
  bool GetWasPreviouslyInstalled() const;
  bool GetContainsAds() const;
  bool GetOptimizedForChrome() const;

 private:
  std::string package_name_;
  GURL icon_url_;
  std::u16string category_;
  std::u16string description_;
  std::u16string content_rating_;
  GURL content_rating_icon_url_;
  bool has_in_app_purchases_;
  bool was_previously_installed_;
  bool contains_ads_;
  bool optimized_for_chrome_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_PLAY_EXTRAS_H_
