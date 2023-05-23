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

signin::Tribool GetPrivacySandboxRestrictedByAccountCapability(
    signin::IdentityManager* identity_manager) {
  const auto core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  return account_info.capabilities.can_run_chrome_privacy_sandbox_trials();
}
}  // namespace

PrivacySandboxSettingsDelegate::PrivacySandboxSettingsDelegate(Profile* profile)
    : profile_(profile) {}

PrivacySandboxSettingsDelegate::~PrivacySandboxSettingsDelegate() = default;

bool PrivacySandboxSettingsDelegate::IsPrivacySandboxRestricted() const {
  if (privacy_sandbox::kPrivacySandboxSettings4ForceRestrictedUserForTesting
          .Get()) {
    return true;
  }
  // If the Sandbox was ever reported as restricted, it is always restricted.
  // TODO (crbug.com/1428546): Adjust when we have a graduation flow.
  bool was_ever_reported_as_restricted =
      profile_->GetPrefs()->GetBoolean(prefs::kPrivacySandboxM1Restricted);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user isn't signed in so we can't apply any capabilties-based
    // restrictions.
    return was_ever_reported_as_restricted;
  }

  auto restricted_by_capability =
      GetPrivacySandboxRestrictedByAccountCapability(identity_manager);

  // The Privacy Sandbox is not considered restricted unless the
  // capability has a definitive false signal.
  bool is_restricted = restricted_by_capability == signin::Tribool::kFalse;
  // If the capability is restricting the Sandbox, "latch", so the sandbox is
  // always restricted.
  if (is_restricted) {
    profile_->GetPrefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);
  }

  return was_ever_reported_as_restricted || is_restricted;
}

bool PrivacySandboxSettingsDelegate::IsPrivacySandboxCurrentlyUnrestricted()
    const {
  if (privacy_sandbox::kPrivacySandboxSettings4ForceRestrictedUserForTesting
          .Get()) {
    return false;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user isn't signed in so we can't apply any capabilties-based
    // restrictions.
    return false;
  }

  const AccountInfo account_info =
      identity_manager->FindExtendedPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  auto capability =
      account_info.capabilities.can_run_chrome_privacy_sandbox_trials();
  return capability == signin::Tribool::kTrue;
}

bool PrivacySandboxSettingsDelegate::IsSubjectToM1NoticeRestricted() const {
  // If the feature is deactivated, the notice shouldn't be shown.
  if (!privacy_sandbox::kPrivacySandboxSettings4RestrictedNotice.Get()) {
    return false;
  }
  return PrivacySandboxRestrictedNoticeRequired();
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

bool PrivacySandboxSettingsDelegate::PrivacySandboxRestrictedNoticeRequired()
    const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);

  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // The user isn't signed in so we can't apply any capabilties-based
    // restrictions.
    return false;
  }

  const AccountInfo account_info =
      identity_manager->FindExtendedPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  auto capability =
      account_info.capabilities
          .is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice();
  return capability == signin::Tribool::kTrue;
}
