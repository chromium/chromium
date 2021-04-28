// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

bool IsCookiesClearOnExitEnabled(HostContentSettingsMap* map) {
  return map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                       /*provider_id=*/nullptr) ==
         ContentSetting::CONTENT_SETTING_SESSION_ONLY;
}

bool HasNonDefaultBlockSetting(const ContentSettingsForOneType& cookie_settings,
                               const GURL& url,
                               const GURL& top_frame_origin) {
  // APIs are allowed unless there is an effective non-default cookie content
  // setting block exception. A default cookie content setting is one that has a
  // wildcard pattern for both primary and secondary patterns. Content
  // settings are listed in descending order of priority such that the first
  // that matches is the effective content setting. A default setting can appear
  // anywhere in the list. Content settings which appear after a default content
  // setting are completely superseded by that content setting and are thus not
  // consulted. Default settings which appear before other settings are applied
  // from higher precedence sources, such as policy. The value of a default
  // content setting applied by a higher precedence provider is not consulted
  // here. For managed policies, the state will be reflected directly in the
  // privacy sandbox preference. Other providers (such as extensions) will have
  // been considered for the initial value of the privacy sandbox preference.
  for (const auto& setting : cookie_settings) {
    if (setting.primary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      return false;
    }
    if (setting.primary_pattern.Matches(url) &&
        setting.secondary_pattern.Matches(top_frame_origin)) {
      return setting.GetContentSetting() ==
             ContentSetting::CONTENT_SETTING_BLOCK;
    }
  }
  // ContentSettingsForOneType should always end with a default content setting
  // from the default provider.
  NOTREACHED();
  return false;
}

// Returns true iff based on |cookies_settings| & |prefs| third party cookies
// are disabled by policy. This includes disabling third party cookies via
// disabling all cookies.
bool ThirdPartyCookiesDisabledByPolicy(
    content_settings::CookieSettings* cookie_settings,
    PrefService* prefs) {
  auto* cookie_controls_mode_pref =
      prefs->FindPreference(prefs::kCookieControlsMode);
  auto cookie_controls_mode_value =
      static_cast<content_settings::CookieControlsMode>(
          cookie_controls_mode_pref->GetValue()->GetInt());

  if (cookie_controls_mode_pref->IsManaged() &&
      cookie_controls_mode_value ==
          content_settings::CookieControlsMode::kBlockThirdParty) {
    return true;
  }

  std::string default_cookie_setting_provider;
  auto default_cookie_setting = cookie_settings->GetDefaultCookieSetting(
      &default_cookie_setting_provider);
  auto default_cookie_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          default_cookie_setting_provider);

  if (default_cookie_setting_source ==
          content_settings::SettingSource::SETTING_SOURCE_POLICY &&
      default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    return true;
  }

  return false;
}

// Returns whether |cookie_settings| and |prefs| imply that a user's Privacy
// Sandbox preference should be turned off.
bool ShouldDisablePrivacySandbox(
    content_settings::CookieSettings* cookie_settings,
    PrefService* prefs) {
  // If a user has already expressed control over the Privacy Sandbox preference
  // on any of their devices there is no need to disable it.
  if (prefs->GetBoolean(prefs::kPrivacySandboxManuallyControlled))
    return false;

  auto cookie_controls_mode_value =
      static_cast<content_settings::CookieControlsMode>(
          prefs->GetInteger(prefs::kCookieControlsMode));

  auto default_cookie_setting =
      cookie_settings->GetDefaultCookieSetting(/*provider_id=*/nullptr);

  // The Privacy Sandbox preference should be disabled if 3P cookies or all
  // cookies are blocked.
  return (cookie_controls_mode_value ==
              content_settings::CookieControlsMode::kBlockThirdParty ||
          default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK);
}

}  // namespace

PrivacySandboxSettings::PrivacySandboxSettings(
    HostContentSettingsMap* host_content_settings_map,
    content_settings::CookieSettings* cookie_settings,
    PrefService* pref_service,
    policy::PolicyService* policy_service,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : host_content_settings_map_(host_content_settings_map),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      policy_service_(policy_service),
      sync_service_(sync_service),
      identity_manager_(identity_manager) {
  DCHECK(pref_service_);
  DCHECK(host_content_settings_map_);
  DCHECK(cookie_settings_);
  DCHECK(policy_service_);
  // The |sync_service_| may be null (if sync is explitily disabled, or if the
  // browser context is off the record). A null |idenity_manager_| should only
  // occur if the browser context is off the record, in which case
  // |sync_service_| must also be null.
  DCHECK(identity_manager_ || !sync_service_);

  // "Clear on exit" causes a cookie deletion on shutdown. But for practical
  // purposes, we're notifying the observers on startup (which should be
  // equivalent, as no cookie operations could have happened while the profile
  // was shut down).
  if (IsCookiesClearOnExitEnabled(host_content_settings_map_))
    OnCookiesCleared();

  // On first entering the privacy sandbox experiment, users may have the
  // privacy sandbox disabled (or "reconciled") based on their current cookie
  // settings (e.g. blocking 3P cookies). Depending on the state of the sync
  // service, identity manager, and cookie setting, reconciliation may not run
  // immediately, or may not run at all.
  // TODO(crbug.com/1166665): Remove reconciliation logic when kAPI controls are
  // further separated from cookie controls.
  MaybeReconcilePrivacySandboxPref();
}

PrivacySandboxSettings::~PrivacySandboxSettings() = default;

/*static*/ bool PrivacySandboxSettings::PrivacySandboxSettingsFunctional() {
  return base::FeatureList::IsEnabled(features::kPrivacySandboxSettings);
}

bool PrivacySandboxSettings::IsFlocAllowed(
    const GURL& url,
    const base::Optional<url::Origin>& top_frame_origin) const {
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxAllowedForContext(url, top_frame_origin,
                                           cookie_settings);
}

base::Time PrivacySandboxSettings::FlocDataAccessibleSince() const {
  return pref_service_->GetTime(prefs::kPrivacySandboxFlocDataAccessibleSince);
}

bool PrivacySandboxSettings::IsConversionMeasurementAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) const {
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxAllowedForContext(reporting_origin.GetURL(),
                                           top_frame_origin, cookie_settings);
}

bool PrivacySandboxSettings::ShouldSendConversionReport(
    const url::Origin& impression_origin,
    const url::Origin& conversion_origin,
    const url::Origin& reporting_origin) const {
  // Re-using the |cookie_settings| allows this function to be faster
  // than simply calling IsConversionMeasurementAllowed() twice
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  // The |reporting_origin| needs to have been accessible in both impression
  // and conversion contexts. These are both checked when they occur, but
  // user settings may have changed between then and when the conversion report
  // is sent.
  return IsPrivacySandboxAllowedForContext(
             reporting_origin.GetURL(), impression_origin, cookie_settings) &&
         IsPrivacySandboxAllowedForContext(reporting_origin.GetURL(),
                                           conversion_origin, cookie_settings);
}

bool PrivacySandboxSettings::IsFledgeAllowed(
    const url::Origin& top_frame_origin,
    const GURL& auction_party) {
  // If the sandbox is available and disabled, then FLEDGE is never allowed.
  if (base::FeatureList::IsEnabled(features::kPrivacySandboxSettings) &&
      !pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled)) {
    return false;
  }

  // Third party cookies must also be available for this context. An empty site
  // for cookies is provided so the context is always treated as a third party.
  return cookie_settings_->IsCookieAccessAllowed(auction_party, GURL(),
                                                 top_frame_origin);
}

std::vector<GURL> PrivacySandboxSettings::FilterFledgeAllowedParties(
    const url::Origin& top_frame_origin,
    const std::vector<GURL>& auction_parties) {
  // If the sandbox is available and disabled, then no parties are allowed.
  if (base::FeatureList::IsEnabled(features::kPrivacySandboxSettings) &&
      !pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled)) {
    return {};
  }

  std::vector<GURL> allowed_parties;
  for (const auto& party : auction_parties) {
    if (cookie_settings_->IsCookieAccessAllowed(party, GURL(),
                                                top_frame_origin)) {
      allowed_parties.push_back(party);
    }
  }
  return allowed_parties;
}

bool PrivacySandboxSettings::IsPrivacySandboxAllowed() {
  if (!PrivacySandboxSettingsFunctional()) {
    // Simply respect 3rd-party cookies blocking settings if the UI is not
    // available.
    return !cookie_settings_->ShouldBlockThirdPartyCookies();
  }

  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

bool PrivacySandboxSettings::IsPrivacySandboxEnabled() {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

bool PrivacySandboxSettings::IsPrivacySandboxManaged() {
  return pref_service_->IsManagedPreference(prefs::kPrivacySandboxApisEnabled);
}

void PrivacySandboxSettings::SetPrivacySandboxEnabled(bool enabled) {
  if (!base::FeatureList::IsEnabled(features::kPrivacySandboxSettings)) {
    return;
  }
  pref_service_->SetBoolean(prefs::kPrivacySandboxManuallyControlled, true);
  pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabled, enabled);
}

void PrivacySandboxSettings::OnCookiesCleared() {
  pref_service_->SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince,
                         base::Time::Now());

  for (auto& observer : observers_) {
    observer.OnFlocDataAccessibleSinceUpdated();
  }
}

void PrivacySandboxSettings::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacySandboxSettings::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PrivacySandboxSettings::Shutdown() {
  StopObserving();
}

void PrivacySandboxSettings::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                             const policy::PolicyMap& previous,
                                             const policy::PolicyMap& current) {
  // |pref_service_| and |cookie_settings_| will have been made aware
  // of the policy changes before this observer function is called.
  MaybeReconcilePrivacySandboxPref();
}

void PrivacySandboxSettings::OnStateChanged(syncer::SyncService* sync) {
  MaybeReconcilePrivacySandboxPref();
}

void PrivacySandboxSettings::OnSyncCycleCompleted(syncer::SyncService* sync) {
  MaybeReconcilePrivacySandboxPref();
}

void PrivacySandboxSettings::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  MaybeReconcilePrivacySandboxPref();
}

bool PrivacySandboxSettings::IsPrivacySandboxAllowedForContext(
    const GURL& url,
    const base::Optional<url::Origin>& top_frame_origin,
    const ContentSettingsForOneType& cookie_settings) const {
  if (!base::FeatureList::IsEnabled(features::kPrivacySandboxSettings)) {
    // Simply respect cookie settings if the UI is not available. An empty site
    // for cookies is provided so the context is always as a third party.
    return cookie_settings_->IsCookieAccessAllowed(url, GURL(),
                                                   top_frame_origin);
  }

  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return false;

  // TODO (crbug.com/1155504): Bypassing the CookieSettings class to access
  // content settings directly ignores allowlisted schemes and the storage
  // access API. These should be taken into account here.
  return !HasNonDefaultBlockSetting(
      cookie_settings, url,
      top_frame_origin ? top_frame_origin->GetURL() : GURL());
}

void PrivacySandboxSettings::MaybeReconcilePrivacySandboxPref() {
  // No action required if the user does not have the UI available.
  if (!PrivacySandboxSettingsFunctional())
    return;

  // No need to reconcile preferences if it has already happened.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxPreferencesReconciled)) {
    LogPrivacySandboxState();
    return;
  }

  // If all or 3P cookies are disabled by policy, this will be reflected
  // directly in the Privacy Sandbox preference at the policy level. No attempt
  // should be made to reconcile the user preference while this is true, as due
  // to sync this may opt a user out on a personal device based on managed
  // device settings. If the device becomes unmanaged, or the policy changes,
  // reconciliation should occur.
  if (ThirdPartyCookiesDisabledByPolicy(cookie_settings_, pref_service_)) {
    // The policy service may already be observed, e.g. if this method is being
    // called after an update which did not result in reconciliation running.
    if (!policy_service_observed_) {
      policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
      policy_service_observed_ = true;
      LogPrivacySandboxState();
    }
    return;
  }

  // Reconciliation of the Privacy Sandbox preference is based on both synced
  // and unsynced settings. The synced settings are only consulted should the
  // local settings indicate the Privacy Sandbox should be disabled.
  if (!ShouldDisablePrivacySandbox(cookie_settings_, pref_service_)) {
    ReconcilePrivacySandboxPref();
    return;
  }

  // The current settings applied to this device indicate that the Privacy
  // Sandbox should be disabled. A decision however cannot be made until it is
  // confirmed that either:
  //   A) the synced state is available, or
  //   B) it has become clear that the sync state will not be available.
  // In both cases reconciliation is run. In outcome A this is obviously fine,
  // in outcome B this risks clobbering some opted in devices if this device
  // would later sync the disabled preference (e.g. by the user signing back
  // into a sync paused device).

  // If the service currently indicates that preferences will not be synced,
  // then outcome B has been reached.
  if (!sync_service_ || !sync_service_->IsSyncFeatureEnabled() ||
      !sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPreferences) ||
      sync_service_->HasUnrecoverableError()) {
    ReconcilePrivacySandboxPref();
    return;
  }

  // If the sync service has already completed a sync cycle, then outcome A has
  // been reached.
  if (sync_service_->HasCompletedSyncCycle()) {
    ReconcilePrivacySandboxPref();
    return;
  }

  // If there is a persistent auth error associated with the primary account's
  // refresh token, then sync will not be able to run and then outcome B has
  // been reached.
  GoogleServiceAuthError auth_error =
      identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync));
  if (auth_error.IsPersistentError()) {
    ReconcilePrivacySandboxPref();
    return;
  }

  // Further tracking to determine when outcome A or B has occurred requires
  // observing both the sync service and the identity manager. It is valid for
  // observation to already be occurring as this method may be called multiple
  // times if observed updates do not result in outcome A or B being reached.
  DCHECK(sync_service_);
  DCHECK(identity_manager_);
  if (!sync_service_observer_.IsObserving())
    sync_service_observer_.Observe(sync_service_);
  if (!identity_manager_observer_.IsObserving())
    identity_manager_observer_.Observe(identity_manager_);
}

void PrivacySandboxSettings::ReconcilePrivacySandboxPref() {
  if (ShouldDisablePrivacySandbox(cookie_settings_, pref_service_))
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);

  pref_service_->SetBoolean(prefs::kPrivacySandboxPreferencesReconciled, true);

  // If observers were setup they are no longer required after reconciliation
  // has occurred.
  StopObserving();
  LogPrivacySandboxState();
}

void PrivacySandboxSettings::StopObserving() {
  // Removing a non-observing observer is a no-op.
  sync_service_observer_.Reset();
  identity_manager_observer_.Reset();
  if (policy_service_observed_) {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    policy_service_observed_ = false;
  }
}

void PrivacySandboxSettings::RecordPrivacySandboxHistogram(
    PrivacySandboxSettings::SettingsPrivacySandboxEnabled state) {
  base::UmaHistogramEnumeration("Settings.PrivacySandbox.Enabled", state);
}

void PrivacySandboxSettings::LogPrivacySandboxState() {
  // Check policy status first.
  std::string default_cookie_setting_provider;
  auto default_cookie_setting = cookie_settings_->GetDefaultCookieSetting(
      &default_cookie_setting_provider);
  auto default_cookie_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          default_cookie_setting_provider);

  if (default_cookie_setting_source ==
          content_settings::SettingSource::SETTING_SOURCE_POLICY &&
      default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    RecordPrivacySandboxHistogram(
        PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
            kPSDisabledPolicyBlockAll);
    return;
  }

  auto* cookie_controls_mode_pref =
      pref_service_->FindPreference(prefs::kCookieControlsMode);
  auto cookie_controls_mode_value =
      static_cast<content_settings::CookieControlsMode>(
          cookie_controls_mode_pref->GetValue()->GetInt());

  if (cookie_controls_mode_pref->IsManaged() &&
      cookie_controls_mode_value ==
          content_settings::CookieControlsMode::kBlockThirdParty) {
    RecordPrivacySandboxHistogram(
        PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
            kPSDisabledPolicyBlock3P);
    return;
  }

  if (pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled)) {
    if (default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
              kPSEnabledBlockAll);
    } else if (cookie_controls_mode_value ==
               content_settings::CookieControlsMode::kBlockThirdParty) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
              kPSEnabledBlock3P);
    } else {
      RecordPrivacySandboxHistogram(
          PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
              kPSEnabledAllowAll);
    }
  } else {
    if (default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
              kPSDisabledBlockAll);
    } else if (cookie_controls_mode_value ==
               content_settings::CookieControlsMode::kBlockThirdParty) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
              kPSDisabledBlock3P);
    } else {
      RecordPrivacySandboxHistogram(
          PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
              kPSDisabledAllowAll);
    }
  }
}
