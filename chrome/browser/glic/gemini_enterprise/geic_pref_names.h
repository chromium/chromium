// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_PREF_NAMES_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_PREF_NAMES_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Pref names for Gemini Enterprise in Chrome (GEiC). Persisted names use the
// "geic." prefix.
namespace geic::prefs {

// Values for the per-endpoint "status" entries in `kGeicCompletedFre`.
// Persisted as int; entries must never be renumbered. Mirrors
// `glic::prefs::FreStatus`, but is a separate type so that a GEiC status
// can't be passed to consumer GLiC APIs by mistake. Unknown values read as
// `kNotStarted`.
enum class GeicFreStatus : int {
  kMinValue = 0,

  kNotStarted = kMinValue,
  kCompleted = 1,

  kMaxValue = kCompleted,
};

// Dict pref that records the GEiC FRE status per GE instance. Keyed by the
// guest URL without the UI-locale `hl` param (e.g.
// "https://business.gemini.google/side-panel?configId=abc"); see geic_fre.cc.
// Each value is a dict of {"status": int GeicFreStatus}.
// Profile-scoped and not synced, like `glic::prefs::kGlicCompletedFre`. The
// status is profile-level consent to use GEiC with a GE instance, so it is not
// keyed by, or reset for, the GE account signed in within the side panel.
inline constexpr char kGeicCompletedFre[] = "geic.completed_fre";

// Registers GEiC profile prefs. Called from `RegisterProfilePrefs()` in
// chrome/browser/prefs/browser_prefs.cc.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace geic::prefs

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_PREF_NAMES_H_
