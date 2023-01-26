// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace {

bool PrivacySandboxRestrictedByAcccountCapability(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user isn't signed in so we can't apply any capabilties-based
    // restrictions.
    return false;
  }

  const auto core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  auto capability =
      account_info.capabilities.can_run_chrome_privacy_sandbox_trials();

  // The Privacy Sandbox is not considered restricted unless the capability
  // has a definitive false signal.
  return capability == signin::Tribool::kFalse;
}

}  // namespace

PrivacySandboxSettingsDelegate::PrivacySandboxSettingsDelegate(Profile* profile)
    : profile_(profile) {}

PrivacySandboxSettingsDelegate::~PrivacySandboxSettingsDelegate() = default;

bool PrivacySandboxSettingsDelegate::IsPrivacySandboxRestricted() const {
  return PrivacySandboxRestrictedByAcccountCapability(profile_);
}

bool PrivacySandboxSettingsDelegate::IsIncognitoProfile() const {
  return profile_->IsIncognitoProfile();
}

bool PrivacySandboxSettingsDelegate::HasAppropriateTopicsConsent() const {
  // If the profile doesn't require a release 4 consent, then it always has
  // an appropriate (i.e. not required) Topics consent.
  if (!privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get()) {
    return true;
  }

  // Ideally we could consult the PrivacySandboxService, and centralise this
  // logic. However, that service depends on PrivacySandboxSettings, which will
  // own this delegate, and so including it here would create a circular
  // dependency.
  return profile_->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxTopicsConsentGiven);
}
