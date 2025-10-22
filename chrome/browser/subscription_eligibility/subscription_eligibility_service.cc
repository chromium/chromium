// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"

#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/prefs/pref_service.h"

namespace subscription_eligibility {

SubscriptionEligibilityService::SubscriptionEligibilityService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      prefs::kAiSubscriptionTier,
      base::BindRepeating(
          &SubscriptionEligibilityService::OnAiSubscriptionTierUpdated,
          base::Unretained(this)));
}
SubscriptionEligibilityService::~SubscriptionEligibilityService() = default;

int32_t SubscriptionEligibilityService::GetAiSubscriptionTier() {
  return pref_service_->GetInteger(prefs::kAiSubscriptionTier);
}

void SubscriptionEligibilityService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SubscriptionEligibilityService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SubscriptionEligibilityService::OnAiSubscriptionTierUpdated() {
  for (Observer& observer : observers_) {
    observer.OnAiSubscriptionTierUpdated(GetAiSubscriptionTier());
  }
}

}  // namespace subscription_eligibility
