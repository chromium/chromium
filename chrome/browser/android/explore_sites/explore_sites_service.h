// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_H_

#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace explore_sites {

// Main class and entry point for the Explore Sites feature, that
// controls the lifetime of all major subcomponents.
class ExploreSitesService : public KeyedService {
 public:
  ~ExploreSitesService() override = default;

  // Returns via callback the current catalog stored locally.
  virtual void GetCatalog(CatalogCallback callback) = 0;

  // Returns via callback the image for a category. This image is composed from
  // multiple site images. The site images are checked against the user
  // blacklist so that unwanted sites are not represented in the category image.
  // Returns |nullptr| if there was an error, or no match.
  virtual void GetCategoryImage(int category_id,
                                int pixel_size,
                                BitmapCallback callback) = 0;

  // Returns via callback the image for a site. This is typically the site
  // favicon. Returns |nullptr| if there was an error or no match for |site_id|.
  virtual void GetSiteImage(int site_id, BitmapCallback callback) = 0;

  // Fetch the latest catalog from the network and stores it locally. Returns
  // true in the callback for success.  |is_immediate_fetch| denotes if the
  // fetch should be done immediately or on background.  If the accept_languages
  // string is empty, no "Accept-Language" header is created for the network
  // request.
  virtual void UpdateCatalogFromNetwork(bool is_immediate_fetch,
                                        const std::string& accept_languages,
                                        BooleanCallback callback) = 0;

  // Add the url to the blacklist.
  virtual void BlacklistSite(const std::string& url) = 0;
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_H_
