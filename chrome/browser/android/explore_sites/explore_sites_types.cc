// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_types.h"

namespace explore_sites {

ExploreSitesSite::ExploreSitesSite(int site_id,
                                   int category_id,
                                   GURL url,
                                   std::string title,
                                   bool is_blocked)
    : site_id(site_id),
      category_id(category_id),
      url(url),
      title(title),
      is_blocked(is_blocked) {}

ExploreSitesSite::ExploreSitesSite(ExploreSitesSite&& other) = default;

ExploreSitesSite::~ExploreSitesSite() = default;

ExploreSitesCategory::ExploreSitesCategory(int category_id,
                                           std::string version_token,
                                           int category_type,
                                           std::string label,
                                           int ntp_shown_count,
                                           int interaction_count)
    : category_id(category_id),
      version_token(version_token),
      category_type(category_type),
      label(label),
      ntp_shown_count(ntp_shown_count),
      interaction_count(interaction_count) {}

ExploreSitesCategory::ExploreSitesCategory(ExploreSitesCategory&& other) =
    default;

ExploreSitesCategory::~ExploreSitesCategory() = default;

}  // namespace explore_sites
