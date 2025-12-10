// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_
#define CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_

class PrefRegistrySimple;
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace glic::prefs {

// ************* LOCAL STATE PREFS ***************
// These prefs are per-Chrome-installation

// Boolean pref that enables or disables the launcher.
inline constexpr char kGlicLauncherEnabled[] = "glic.launcher_enabled";

// String pref that keeps track of the non-localized version of the registered
// hotkey for Glic.
inline constexpr char kGlicLauncherHotkey[] = "glic.launcher_hotkey";

// String pref that keeps track of the non-localized version of the registered
// hotkey for toggling focus between Glic and the browser window.
inline constexpr char kGlicFocusToggleHotkey[] = "glic.focus_toggle_hotkey";

// String pref that keeps track of whether any loaded profile is, or has ever
// been, of a subscription tier that should enable multi-instance.
inline constexpr char kGlicMultiInstanceEnabledBySubscriptionTier[] =
    "glic.multi_instance_enabled_by_tier";

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

// Value enums for the glic.completed_fre pref. Integer pref that determines the
// Fre status for user profile.
enum class FreStatus {
  kMinValue = 0,

  kNotStarted = kMinValue,
  kCompleted = 1,
  kIncomplete = 2,

  kMaxValue = kIncomplete
};

// Boolean pref that determines if the glic button in tabstrip is pinned.
inline constexpr char kGlicPinnedToTabstrip[] = "glic.pinned_to_tabstrip";

// Boolean pref that enables or disables geolocation access for Glic.
inline constexpr char kGlicGeolocationEnabled[] = "glic.geolocation_enabled";
// Boolean pref that enables or disables microphone access for Glic.
inline constexpr char kGlicMicrophoneEnabled[] = "glic.microphone_enabled";
// Boolean pref that enables or disables tab context for Glic.
inline constexpr char kGlicTabContextEnabled[] = "glic.tab_context_enabled";
// Boolean pref that enables or disables tab context for Glic by default when a
// new Glic session starts.
inline constexpr char kGlicDefaultTabContextEnabled[] =
    "glic.default_tab_context_enabled";

// Boolean pref that determines the rollout eligibility for the user profile.
inline constexpr char kGlicRolloutEligibility[] =
    "sync.glic_rollout_eligibility";

// Dict pref that records user status.
inline constexpr char kGlicUserStatus[] = "glic.user_status";

// Integer pref that determines the Fre status for user profile. Values are from
// the FreStatus enum.
inline constexpr char kGlicCompletedFre[] = "glic.completed_fre";

// Time pref that records the last time a user dismissed the Glic window.
inline constexpr char kGlicWindowLastDismissedTime[] =
    "glic.window.last_dimissed_time";

// Integer prefs for the top right corner of the previous window position.
inline constexpr char kGlicPreviousPositionX[] = "glic.previous_bounds.x";
inline constexpr char kGlicPreviousPositionY[] = "glic.previous_bounds.y";

// Bool pref for the closed captioning setting.
inline constexpr char kGlicClosedCaptioningEnabled[] =
    "glic.closed_captioning_enabled";

// Bool pref for the daisy chain new tabs setting.
inline constexpr char kGlicKeepSidepanelOpenOnNewTabsEnabled[] =
    "glic.keep_sidepanel_open_on_new_tabs_enabled";

// Value enums for the "glic.actuation_on_web" pref. Integer pref that
// determines if glic actuation is enabled. This is controlled from the
// enterprise policy.
enum class GlicActuationOnWebPolicyState {
  kMinValue = 0,

  kEnabled = kMinValue,
  kDisabled = 1,

  kMaxValue = kDisabled
};
// This perf is only applicable to enterprise accounts.
inline constexpr char kGlicActuationOnWeb[] = "glic.actuation_on_web";

// Boolean pref for the user enabled actuation on web setting.
inline constexpr char kGlicUserEnabledActuationOnWeb[] =
    "glic.user_enabled_actuation_on_web";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace glic::prefs

#endif  // CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_H_
