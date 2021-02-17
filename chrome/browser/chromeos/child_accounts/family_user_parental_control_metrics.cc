// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_parental_control_metrics.h"

#include <set>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/usage_time_limit_processor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

// UMA histogram FamilyUser.TimeLimitPolicyTypes
// Reports TimeLimitPolicyType which indicates what time limit policies
// are enabled for current Family Link user. Could report multiple buckets if
// multiple policies are enabled.
constexpr char kTimeLimitPolicyTypesHistogramName[] =
    "FamilyUser.TimeLimitPolicyTypes";

}  // namespace

FamilyUserParentalControlMetrics::FamilyUserParentalControlMetrics(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK(profile_->IsChild());
}

FamilyUserParentalControlMetrics::~FamilyUserParentalControlMetrics() = default;

// static
const char* FamilyUserParentalControlMetrics::
    GetTimeLimitPolicyTypesHistogramNameForTest() {
  return kTimeLimitPolicyTypesHistogramName;
}

void FamilyUserParentalControlMetrics::OnNewDay() {
  const base::DictionaryValue* time_limit_prefs =
      profile_->GetPrefs()->GetDictionary(prefs::kUsageTimeLimit);
  DCHECK(time_limit_prefs);

  std::set<TimeLimitPolicyType> enabled_policies;
  usage_time_limit::GetEnabledTimeLimitPolicies(&enabled_policies,
                                                *time_limit_prefs);
  ChildUserService* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(profile_);
  if (child_user_service)
    child_user_service->GetEnabledAppTimeLimitPolicies(&enabled_policies);

  if (enabled_policies.empty()) {
    base::UmaHistogramEnumeration(
        /*name=*/kTimeLimitPolicyTypesHistogramName,
        /*sample=*/TimeLimitPolicyType::kNoTimeLimit);
    return;
  }

  for (const TimeLimitPolicyType& policy : enabled_policies) {
    base::UmaHistogramEnumeration(
        /*name=*/kTimeLimitPolicyTypesHistogramName,
        /*sample=*/policy);
  }
}

}  // namespace chromeos
