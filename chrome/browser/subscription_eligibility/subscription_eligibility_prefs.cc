// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace subscription_eligibility::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      kAiSubscriptionTier, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

}  // namespace subscription_eligibility::prefs
