// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/generated_time_zone_pref_base.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/user_manager/user_manager.h"

namespace extensions {

namespace settings_api = api::settings_private;

namespace settings_private {

GeneratedTimeZonePrefBase::GeneratedTimeZonePrefBase(
    const std::string& pref_name,
    Profile* profile)
    : pref_name_(pref_name), profile_(profile) {
  g_browser_process->platform_part()->GetTimezoneResolverManager()->AddObserver(
      this);
}

GeneratedTimeZonePrefBase::~GeneratedTimeZonePrefBase() {
  g_browser_process->platform_part()
      ->GetTimezoneResolverManager()
      ->RemoveObserver(this);
}

void GeneratedTimeZonePrefBase::OnTimeZoneResolverUpdated() {
  NotifyObservers(pref_name_);
}

void GeneratedTimeZonePrefBase::UpdateTimeZonePrefControlledBy(
    settings_api::PrefObject* out_pref) const {
  if (chromeos::system::TimeZoneResolverManager::
          IsTimeZoneResolutionPolicyControlled()) {
    out_pref->controlled_by = settings_api::CONTROLLED_BY_DEVICE_POLICY;
    out_pref->enforcement = settings_api::ENFORCEMENT_ENFORCED;
  } else if (profile_->IsChild()) {
    out_pref->controlled_by = settings_api::CONTROLLED_BY_PARENT;
    if (base::FeatureList::IsEnabled(
            features::kParentAccessCodeForTimeChange)) {
      out_pref->enforcement = settings_api::ENFORCEMENT_PARENT_SUPERVISED;
    } else {
      out_pref->enforcement = settings_api::ENFORCEMENT_ENFORCED;
    }
  } else if (!profile_->IsSameProfile(
                 ProfileManager::GetPrimaryUserProfile())) {
    out_pref->controlled_by = settings_api::CONTROLLED_BY_PRIMARY_USER;
    out_pref->controlled_by_name = std::make_unique<std::string>(
        user_manager::UserManager::Get()->GetPrimaryUser()->GetDisplayEmail());
    out_pref->enforcement = settings_api::ENFORCEMENT_ENFORCED;
  }
  // Time zone settings can be policy-bound (for all users), or primary-user
  // bound (for secondary users in multiprofile mode). Otherwise do not modify
  // default values.
}

}  // namespace settings_private
}  // namespace extensions
