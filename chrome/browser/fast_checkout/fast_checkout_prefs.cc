// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_prefs.h"

#include "base/command_line.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {
// Copy from autofill_assistant::switches::kAutofillAssistantForceOnboarding.
const char kAutofillAssistantForceOnboarding[] =
    "autofill-assistant-force-onboarding";
}  // namespace

namespace prefs {
// Indicates whether a user has declined to give consent to Fast Checkout's
// onboarding process.
const char kFastCheckoutOnboardingDeclined[] =
    "fast_checkout.onboarding_declined";
}  // namespace prefs

FastCheckoutPrefs::FastCheckoutPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {}

void FastCheckoutPrefs::DeclineOnboarding() {
  pref_service_->SetBoolean(prefs::kFastCheckoutOnboardingDeclined, true);
}

bool FastCheckoutPrefs::IsOnboardingDeclined() {
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kAutofillAssistantForceOnboarding) == "true") {
    // Ignore whether onboarding was declined if onboarding is forced to be
    // shown. This is only for developing and testing purposes.
    return false;
  }
  return pref_service_->GetBoolean(prefs::kFastCheckoutOnboardingDeclined);
}

// static
void FastCheckoutPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFastCheckoutOnboardingDeclined, false);
}
