// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_PREFS_H_
#define CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace subscription_eligibility::prefs {

// ************* PROFILE PREFS ***************
// Prefs below are tied to a user profile

// Integer pref that determines the rollout eligibility for the user profile.
inline constexpr char kAiSubscriptionTier[] = "sync.ai_subscription_tier";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace subscription_eligibility::prefs

#endif  // CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_PREFS_H_
