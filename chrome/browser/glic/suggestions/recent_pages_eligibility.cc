// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/recent_pages_eligibility.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace glic {
namespace {

bool IsGlicTabContextEnabled(const PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting)) {
    return false;
  }
  return pref_service->GetBoolean(prefs::kGlicDefaultTabContextEnabled);
}

}  // namespace

RecentPagesIneligibilityReason GetRecentPagesEligibility(Profile* profile) {
  CHECK(profile);
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service || !sync_service->GetUserSettings()->GetSelectedTypes().Has(
                           syncer::UserSelectableType::kHistory)) {
    return RecentPagesIneligibilityReason::kHistorySyncOff;
  }
  if (!GlicEnabling::HasConsentedForProfile(profile)) {
    return RecentPagesIneligibilityReason::kFreNotAccepted;
  }
  if (!IsGlicTabContextEnabled(profile->GetPrefs())) {
    return RecentPagesIneligibilityReason::kTabContextDisabled;
  }
  const signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return RecentPagesIneligibilityReason::kAgeRestricted;
  }
  const AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  if (!GlicEnabling::CanUseAdultFeatures(
          account_info.GetAccountCapabilities())) {
    return RecentPagesIneligibilityReason::kAgeRestricted;
  }
  return RecentPagesIneligibilityReason::kNone;
}

}  // namespace glic
