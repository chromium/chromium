// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_PREFS_NTP_PREFS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_PREFS_NTP_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace ntp_prefs {

// Registers the New Tab Page profile prefs declared in `ntp_pref_names.h` that
// aren't owned by a specific NTP component.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Resets the prefs registered by `RegisterProfilePrefs()` (except the
// `kNtpPrevNavigationTime` metrics timestamp) and the context menu animation
// state to their defaults. Prefs owned by NTP
// components (e.g. `MostVisitedPrefObserver::ResetProfilePrefs()`) are reset
// separately; `ProfileResetter::ResetNtpCustomizations()` resets them all.
void ResetProfilePrefs(PrefService* prefs);

}  // namespace ntp_prefs

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_PREFS_NTP_PREFS_H_
