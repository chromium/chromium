// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_TILES_CHROME_POPULAR_SITES_FACTORY_H_
#define CHROME_BROWSER_NTP_TILES_CHROME_POPULAR_SITES_FACTORY_H_

#include <memory>

class Profile;

namespace ntp_tiles {
class PopularSites;
}  // namespace ntp_tiles

class ChromePopularSitesFactory {
 public:
  static std::unique_ptr<ntp_tiles::PopularSites> NewForProfile(
      Profile* profile);

  ChromePopularSitesFactory() = delete;
};

#endif  // CHROME_BROWSER_NTP_TILES_CHROME_POPULAR_SITES_FACTORY_H_
