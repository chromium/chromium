// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"

#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/prefs/pref_service.h"

namespace subscription_eligibility {

SubscriptionEligibilityService::SubscriptionEligibilityService(
    PrefService* pref_service)
    : pref_service_(pref_service) {}
SubscriptionEligibilityService::~SubscriptionEligibilityService() = default;

int32_t SubscriptionEligibilityService::GetAiSubscriptionTier() {
  return pref_service_->GetInteger(prefs::kAiSubscriptionTier);
}

}  // namespace subscription_eligibility
