// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_types.h"

namespace explore_sites {

ExploreSitesSite::ExploreSitesSite(int site_id,
                                   int category_id,
                                   GURL url,
                                   std::string title)
    : site_id(site_id), category_id(category_id), url(url), title(title) {}

ExploreSitesSite::ExploreSitesSite(ExploreSitesSite&& other) = default;

ExploreSitesSite::~ExploreSitesSite() = default;

ExploreSitesCategory::ExploreSitesCategory(int category_id,
                                           std::string version_token,
                                           int category_type,
                                           std::string label)
    : category_id(category_id),
      version_token(version_token),
      category_type(category_type),
      label(label) {}

ExploreSitesCategory::ExploreSitesCategory(ExploreSitesCategory&& other) =
    default;

ExploreSitesCategory::~ExploreSitesCategory() = default;

}  // namespace explore_sites
