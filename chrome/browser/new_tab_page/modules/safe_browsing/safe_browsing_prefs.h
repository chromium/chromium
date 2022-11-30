// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_SAFE_BROWSING_SAFE_BROWSING_PREFS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_SAFE_BROWSING_SAFE_BROWSING_PREFS_H_

namespace ntp {
namespace prefs {
// Integer (32 bit) counting how many times has the Safe Browsing module been
// shown since the last cooldown.
extern const char kSafeBrowsingModuleShownCount[];

// Int64 epoch timestamp in seconds. Indicates the time the last cooldown
// started.
extern const char kSafeBrowsingModuleLastCooldownStartAt[];

// Boolean that indicates if the user has ever interacted with the module, and
// opened the settings page.
extern const char kSafeBrowsingModuleOpened[];
}  // namespace prefs
}  // namespace ntp

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_SAFE_BROWSING_SAFE_BROWSING_PREFS_H_
