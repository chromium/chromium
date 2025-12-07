// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_TILES_CHROME_ENTERPRISE_SHORTCUTS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_NTP_TILES_CHROME_ENTERPRISE_SHORTCUTS_MANAGER_FACTORY_H_

#include <memory>

class Profile;

namespace ntp_tiles {
class EnterpriseShortcutsManager;
}  // namespace ntp_tiles

class ChromeEnterpriseShortcutsManagerFactory {
 public:
  static std::unique_ptr<ntp_tiles::EnterpriseShortcutsManager> NewForProfile(
      Profile* profile);

  ChromeEnterpriseShortcutsManagerFactory() = delete;
};

#endif  // CHROME_BROWSER_NTP_TILES_CHROME_ENTERPRISE_SHORTCUTS_MANAGER_FACTORY_H_
