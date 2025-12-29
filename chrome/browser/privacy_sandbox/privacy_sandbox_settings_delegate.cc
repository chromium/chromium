// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_confirmation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/common/content_features.h"
#include "net/cookies/cookie_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/webapps/webapp_registry.h"
#endif

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

PrivacySandboxSettingsDelegate::PrivacySandboxSettingsDelegate(
    Profile* profile,
    PrivacySandboxCountries* privacy_sandbox_countries)
    : profile_(profile),
      privacy_sandbox_countries_(privacy_sandbox_countries)
#if BUILDFLAG(IS_ANDROID)
      ,
      webapp_registry_(std::make_unique<WebappRegistry>())
#endif
{
}

PrivacySandboxSettingsDelegate::~PrivacySandboxSettingsDelegate() = default;

bool PrivacySandboxSettingsDelegate::IsRestrictedNoticeEnabled() const {
  return privacy_sandbox::IsRestrictedNoticeRequired(
      privacy_sandbox_countries_);
}

bool PrivacySandboxSettingsDelegate::IsPrivacySandboxRestricted() const {
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
  if (!privacy_sandbox::IsRestrictedNoticeRequired(
          privacy_sandbox_countries_)) {
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
  if (!privacy_sandbox::IsConsentRequired(privacy_sandbox_countries_)) {
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

bool PrivacySandboxSettingsDelegate::IsSubjectToEnterpriseFeatures() const {
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
      account_info.capabilities.is_subject_to_enterprise_features();
  return capability == signin::Tribool::kTrue;
}

#if BUILDFLAG(IS_ANDROID)
void PrivacySandboxSettingsDelegate::OverrideWebappRegistryForTesting(
    std::unique_ptr<WebappRegistry> webapp_registry) {
  DCHECK(webapp_registry);
  webapp_registry_ = std::move(webapp_registry);
}
#endif
