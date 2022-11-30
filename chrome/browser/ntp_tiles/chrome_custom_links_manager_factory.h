// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_TILES_CHROME_CUSTOM_LINKS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_NTP_TILES_CHROME_CUSTOM_LINKS_MANAGER_FACTORY_H_

#include <memory>

class Profile;

namespace ntp_tiles {
class CustomLinksManager;
}  // namespace ntp_tiles

class ChromeCustomLinksManagerFactory {
 public:
  static std::unique_ptr<ntp_tiles::CustomLinksManager> NewForProfile(
      Profile* profile);

  ChromeCustomLinksManagerFactory() = delete;
};

#endif  // CHROME_BROWSER_NTP_TILES_CHROME_CUSTOM_LINKS_MANAGER_FACTORY_H_
