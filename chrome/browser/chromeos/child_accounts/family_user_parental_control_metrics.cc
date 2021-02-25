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
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
namespace chromeos {

namespace {

// UMA histogram FamilyUser.TimeLimitPolicyTypes
// Reports TimeLimitPolicyType which indicates what time limit policies
// are enabled for current Family Link user. Could report multiple buckets if
// multiple policies are enabled.
constexpr char kTimeLimitPolicyTypesHistogramName[] =
    "FamilyUser.TimeLimitPolicyTypes";

// UMA histogram FamilyUser.WebFilterType
// Reports WebFilterType which indicates web filter behaviour are used for
// current Family Link user.
constexpr char kWebFilterTypeHistogramName[] = "FamilyUser.WebFilterType";

// UMA histogram FamilyUser.ManualSiteListType
// Reports ManualSiteListType which indicates approved list and blocked list
// usage for current Family Link user.
constexpr char kManagedSiteListHistogramName[] = "FamilyUser.ManagedSiteList";

}  // namespace

FamilyUserParentalControlMetrics::FamilyUserParentalControlMetrics(
    Profile* profile)
    : profile_(profile),
      first_report_on_current_device_(
          user_manager::UserManager::Get()->IsCurrentUserNew()) {
  DCHECK(profile_);
  DCHECK(profile_->IsChild());
}

FamilyUserParentalControlMetrics::~FamilyUserParentalControlMetrics() = default;

// static
const char* FamilyUserParentalControlMetrics::
    GetTimeLimitPolicyTypesHistogramNameForTest() {
  return kTimeLimitPolicyTypesHistogramName;
}

// static
const char*
FamilyUserParentalControlMetrics::GetWebFilterTypeHistogramNameForTest() {
  return kWebFilterTypeHistogramName;
}

// static
const char*
FamilyUserParentalControlMetrics::GetManagedSiteListHistogramNameForTest() {
  return kManagedSiteListHistogramName;
}

void FamilyUserParentalControlMetrics::ReportTimeLimitPolicy() const {
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
void FamilyUserParentalControlMetrics::ReportWebFilterPolicy() const {
  // Ignores reports when prefs::kDefaultSupervisedUserFilteringBehavior is
  // reset to default value. It might happen during sign out.
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  if (supervised_user_service->IsFilteringBehaviorPrefDefault() ||
      !supervised_user_service->GetURLFilter()) {
    return;
  }

  base::UmaHistogramEnumeration(
      kWebFilterTypeHistogramName,
      supervised_user_service->GetURLFilter()->GetWebFilterType());
  base::UmaHistogramEnumeration(
      kManagedSiteListHistogramName,
      supervised_user_service->GetURLFilter()->GetManagedSiteList());
}

void FamilyUserParentalControlMetrics::OnNewDay() {
  // Reports Family Link user time limit policy type.
  ReportTimeLimitPolicy();

  // Ignores the first report during OOBE. Prefs related to web filter policy
  // may not have been successfully sync during OOBE process, which introduces
  // bias.
  if (first_report_on_current_device_) {
    first_report_on_current_device_ = false;
  } else {
    ReportWebFilterPolicy();
  }
}

}  // namespace chromeos
