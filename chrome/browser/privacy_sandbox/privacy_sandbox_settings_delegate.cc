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
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/common/content_features.h"
#include "net/cookies/cookie_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/webapps/webapp_registry.h"
#endif

namespace {

using TpcdExperimentEligibility = privacy_sandbox::TpcdExperimentEligibility;

signin::Tribool GetPrivacySandboxRestrictedByAccountCapability(
    signin::IdentityManager* identity_manager) {
  const auto core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  return account_info.capabilities.can_run_chrome_privacy_sandbox_trials();
}

const base::FeatureParam<bool> kCookieDeprecationUseProfileFiltering{
    &features::kCookieDeprecationFacilitatedTesting, "use_profile_filtering",
    false};

}  // namespace

PrivacySandboxSettingsDelegate::PrivacySandboxSettingsDelegate(
    Profile* profile,
    tpcd::experiment::ExperimentManager* experiment_manager)
    : profile_(profile),
      experiment_manager_(experiment_manager)
#if BUILDFLAG(IS_ANDROID)
      ,
      webapp_registry_(std::make_unique<WebappRegistry>())
#endif
{
}

PrivacySandboxSettingsDelegate::~PrivacySandboxSettingsDelegate() = default;

bool PrivacySandboxSettingsDelegate::IsRestrictedNoticeEnabled() const {
  return privacy_sandbox::IsRestrictedNoticeRequired();
}

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
  if (!privacy_sandbox::IsRestrictedNoticeRequired()) {
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
  if (!privacy_sandbox::IsConsentRequired()) {
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

bool PrivacySandboxSettingsDelegate::IsCookieDeprecationExperimentEligible()
    const {
  if (!base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return false;
  }

  if (!features::kCookieDeprecationFacilitatedTestingEnableOTRProfiles.Get() &&
      (profile_->IsOffTheRecord() || profile_->IsGuestSession())) {
    return false;
  }

  // Uses per-profile filtering if enabled.
  if (kCookieDeprecationUseProfileFiltering.Get()) {
    // The 3PCD experiment eligibility persists for the browser session.
    if (!is_cookie_deprecation_experiment_eligible_.has_value()) {
      is_cookie_deprecation_experiment_eligible_ =
          GetCookieDeprecationExperimentCurrentEligibility().is_eligible();
    }

    return *is_cookie_deprecation_experiment_eligible_;
  }

  if (experiment_manager_) {
    return experiment_manager_->IsClientEligible().value_or(false);
  }

  return false;
}

TpcdExperimentEligibility PrivacySandboxSettingsDelegate::
    GetCookieDeprecationExperimentCurrentEligibility() const {
  if (tpcd::experiment::kForceEligibleForTesting.Get()) {
    return TpcdExperimentEligibility(
        TpcdExperimentEligibility::Reason::kForcedEligible);
  }

  // Whether third-party cookies are blocked.
  if (tpcd::experiment::kExclude3PCBlocked.Get()) {
    const auto cookie_controls_mode =
        static_cast<content_settings::CookieControlsMode>(
            profile_->GetPrefs()->GetInteger(prefs::kCookieControlsMode));
    if (cookie_controls_mode ==
        content_settings::CookieControlsMode::kBlockThirdParty) {
      return TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::k3pCookiesBlocked);
    }
    scoped_refptr<content_settings::CookieSettings> cookie_settings =
        CookieSettingsFactory::GetForProfile(profile_);
    DCHECK(cookie_settings);
    if (cookie_settings->GetDefaultCookieSetting() ==
        ContentSetting::CONTENT_SETTING_BLOCK) {
      return TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::k3pCookiesBlocked);
    }
  }

  // Whether the privacy sandbox Ads APIs notice has been seen.
  //
  // TODO(linnan): Consider checking whether the restricted notice has been
  // acknowledged (`prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged`) as
  // well.
  if (tpcd::experiment::kExcludeNotSeenAdsAPIsNotice.Get()) {
    const bool row_notice_acknowledged = profile_->GetPrefs()->GetBoolean(
        prefs::kPrivacySandboxM1RowNoticeAcknowledged);
    const bool eaa_notice_acknowledged = profile_->GetPrefs()->GetBoolean(
        prefs::kPrivacySandboxM1EEANoticeAcknowledged);
    if (!row_notice_acknowledged && !eaa_notice_acknowledged) {
      return TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::kHasNotSeenNotice);
    }
  }

  // Whether it's a dasher account.
  if (tpcd::experiment::kExcludeDasherAccount.Get() &&
      IsSubjectToEnterprisePolicies()) {
    return TpcdExperimentEligibility(
        TpcdExperimentEligibility::Reason::kEnterpriseUser);
  }

  // TODO(linnan): Consider moving the following client-level filtering to
  // `ExperimentManager`.

  // Whether it's a new client.
  if (tpcd::experiment::kExcludeNewUser.Get()) {
    base::Time install_date =
        base::Time::FromTimeT(g_browser_process->local_state()->GetInt64(
            metrics::prefs::kInstallDate));
    if (install_date.is_null() ||
        base::Time::Now() - install_date <
            tpcd::experiment::kInstallTimeForNewUser.Get()) {
      return TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::kNewUser);
    }
  }

// Whether PWA or TWA has been installed on Android.
#if BUILDFLAG(IS_ANDROID)
  if (tpcd::experiment::kExcludePwaOrTwaInstalled.Get() &&
      !webapp_registry_->GetOriginsWithInstalledApp().empty()) {
    return TpcdExperimentEligibility(
        TpcdExperimentEligibility::Reason::kPwaOrTwaInstalled);
  }
#endif

  return TpcdExperimentEligibility(
      TpcdExperimentEligibility::Reason::kEligible);
}

bool PrivacySandboxSettingsDelegate::IsSubjectToEnterprisePolicies() const {
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
      account_info.capabilities.is_subject_to_enterprise_policies();
  return capability == signin::Tribool::kTrue;
}

#if BUILDFLAG(IS_ANDROID)
void PrivacySandboxSettingsDelegate::OverrideWebappRegistryForTesting(
    std::unique_ptr<WebappRegistry> webapp_registry) {
  DCHECK(webapp_registry);
  webapp_registry_ = std::move(webapp_registry);
}
#endif

bool PrivacySandboxSettingsDelegate::IsCookieDeprecationLabelAllowed() const {
  if (!IsCookieDeprecationExperimentEligible()) {
    return false;
  }

  auto* tracking_protection_onboarding =
      TrackingProtectionOnboardingFactory::GetForProfile(profile_);
  if (!tracking_protection_onboarding) {
    return false;
  }

  if (tpcd::experiment::kDisable3PCookies.Get()) {
    switch (tracking_protection_onboarding->GetOnboardingStatus()) {
      case privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
          kIneligible:
        return false;
      case privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
          kEligible:
        return !tpcd::experiment::kNeedOnboardingForLabel.Get();
      case privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
          kOnboarded:
        return true;
    }
  } else if (tpcd::experiment::kEnableSilentOnboarding.Get()) {
    switch (tracking_protection_onboarding->GetSilentOnboardingStatus()) {
      case privacy_sandbox::TrackingProtectionOnboarding::
          SilentOnboardingStatus::kIneligible:
        return false;
      case privacy_sandbox::TrackingProtectionOnboarding::
          SilentOnboardingStatus::kEligible:
        return !tpcd::experiment::kNeedOnboardingForLabel.Get();
      case privacy_sandbox::TrackingProtectionOnboarding::
          SilentOnboardingStatus::kOnboarded:
        return true;
    }
  } else {
    return true;
  }
}

bool PrivacySandboxSettingsDelegate::
    AreThirdPartyCookiesBlockedByCookieDeprecationExperiment() const {
  if (net::cookie_util::IsForceThirdPartyCookieBlockingEnabled()) {
    return false;
  }

  if (!IsCookieDeprecationExperimentEligible()) {
    return false;
  }

  if (!tpcd::experiment::kDisable3PCookies.Get()) {
    return false;
  }

  auto* tracking_protection_onboarding =
      TrackingProtectionOnboardingFactory::GetForProfile(profile_);
  if (!tracking_protection_onboarding) {
    return false;
  }

  // Third-party cookies are not disabled until the profile gets onboarded.
  switch (tracking_protection_onboarding->GetOnboardingStatus()) {
    case privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
        kIneligible:
    case privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
        kEligible:
      return false;
    case privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
        kOnboarded:
      break;
  }

  // Respect user preferences.

  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(profile_);
  if (tracking_protection_settings &&
      tracking_protection_settings->AreAllThirdPartyCookiesBlocked()) {
    return false;
  }

  const auto cookie_controls_mode =
      static_cast<content_settings::CookieControlsMode>(
          profile_->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  switch (cookie_controls_mode) {
    case content_settings::CookieControlsMode::kBlockThirdParty:
    case content_settings::CookieControlsMode::kLimited:
      return false;
    case content_settings::CookieControlsMode::kIncognitoOnly:
    case content_settings::CookieControlsMode::kOff:
      break;
  }

  return true;
}
