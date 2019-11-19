// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_MOST_VISITED_CLIENT_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_MOST_VISITED_CLIENT_H_

#include <memory>

#include "components/ntp_tiles/most_visited_sites.h"

namespace explore_sites {

// Takes a URL that the user has asked us to remove, and adds it to a blacklist
// of sites we will stop showing in Explore on Sites.
class MostVisitedClient
    : public ntp_tiles::MostVisitedSites::ExploreSitesClient {
 public:
  static std::unique_ptr<MostVisitedClient> Create();
  ~MostVisitedClient() override;

  GURL GetExploreSitesUrl() const override;
  base::string16 GetExploreSitesTitle() const override;

 private:
  MostVisitedClient();
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_MOST_VISITED_CLIENT_H_
