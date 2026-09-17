// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_PREF_NAMES_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_PREF_NAMES_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace geic::prefs {

// Boolean pref that determines if the GEiC button in the tabstrip is pinned.
inline constexpr char kGeicPinnedToTabstrip[] = "geic.pinned_to_tabstrip";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace geic::prefs

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_PREF_NAMES_H_
