// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_
#define CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_

class PrefRegistrySimple;

namespace glic::prefs {

// ************* LOCAL STATE PREFS ***************
// These prefs are per-Chrome-installation

// Boolean pref that enables or disables the launcher.
inline constexpr char kGlicLauncherEnabled[] = "glic.launcher_enabled";

// String pref that keeps track of the non-localized version of the registered
// hotkey for Glic.
inline constexpr char kGlicLauncherHotkey[] = "glic.launcher_hotkey";

// ************* PROFILE PREFS ***************
// Prefs below are tied to a user profile

// Value enums for the browser.gemini_settings pref. Integer pref that
// determines Glic enabling state for this user profile. This is controlled from
// enterprise policy.
// TODO(crbug.com/393537628): This should be moved to a less Glic-specific
// place.
enum class SettingsPolicyState {
  kMinValue = 0,

  kEnabled = kMinValue,
  kDisabled = 1,

  kMaxValue = kDisabled
};

// Boolean pref that determines if the glic button in tabstrip is pinned.
inline constexpr char kGlicPinnedToTabstrip[] = "glic.pinned_to_tabstrip";

// Boolean pref that enables or disables geolocation access for Glic.
inline constexpr char kGlicGeolocationEnabled[] = "glic.geolocation_enabled";
// Boolean pref that enables or disables microphone access for Glic.
inline constexpr char kGlicMicrophoneEnabled[] = "glic.microphone_enabled";
// Boolean pref that enables or disables tab context for Glic.
inline constexpr char kGlicTabContextEnabled[] = "glic.tab_context_enabled";

// Boolean pref that tracks whether the Glic FRE was completed for this user
// profile.
inline constexpr char kGlicCompletedFre[] = "glic.completed_fre";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace glic::prefs

#endif  // CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_
