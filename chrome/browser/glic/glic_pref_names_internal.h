// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_INTERNAL_H_
#define CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_INTERNAL_H_

namespace glic::prefs {

// Access to these prefs should be guarded. The only place that should access
// them directly (including in tests) is GlicEnabling.

// Integer pref that determines the FRE status for the user profile. Values are
// from the FreStatus enum.
inline constexpr char kGlicCompletedFre[] = "glic.completed_fre";

// Boolean pref for the user-enabled actuation on web setting.
inline constexpr char kGlicUserEnabledActuationOnWeb[] =
    "glic.user_enabled_actuation_on_web";

// Boolean pref that enables or disables experimental triggering.
inline constexpr char kGlicExperimentalTriggeringEnabled[] =
    "glic.experimental_triggering_enabled";

// Integer pref that tracks onboarding interaction status.
inline constexpr char kGlicOnboardingStatus[] = "glic.onboarding_status";
// Time prefs that track last invoked and prompt timestamps.
inline constexpr char kGlicLastInvokedTime[] = "glic.last_invoked_time";
inline constexpr char kGlicLastPromptTime[] = "glic.last_prompt_time";

// Integer pref storing the last observed ProfileReadyState.
inline constexpr char kGlicLastProfileReadyState[] =
    "glic.profile_enablement.last_ready_state";
}  // namespace glic::prefs

#endif  // CHROME_BROWSER_GLIC_GLIC_PREF_NAMES_INTERNAL_H_
