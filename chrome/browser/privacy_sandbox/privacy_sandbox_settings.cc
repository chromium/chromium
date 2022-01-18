// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/federated_learning/features/features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
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

// Returns whether FLoC is allowable by the current state of |pref_service|.
bool IsFlocAllowedByPrefs(PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kPrivacySandboxFlocEnabled) &&
         pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

// Returns the number of days in |time|, rounded to the closest day by hour if
// there is at least 1 day, but rounded to 0 if |time| is less than 1 day.
int GetNumberOfDaysRoundedAboveOne(base::TimeDelta time) {
  int number_of_days = time.InDays();
  if (number_of_days == 0)
    return 0;

  int number_of_hours_past_day = (time - base::Days(number_of_days)).InHours();

  if (number_of_hours_past_day >= 12)
    number_of_days++;

  return number_of_days;
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

  // Register observers for the Privacy Sandbox & FLoC preferences.
  user_prefs_registrar_.Init(pref_service_);
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxApisEnabled,
      base::BindRepeating(&PrivacySandboxSettings::OnPrivacySandboxPrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxFlocEnabled,
      base::BindRepeating(&PrivacySandboxSettings::OnPrivacySandboxPrefChanged,
                          base::Unretained(this)));

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

bool PrivacySandboxSettings::IsFlocAllowed() const {
  return IsFlocAllowedByPrefs(pref_service_);
}

bool PrivacySandboxSettings::IsFlocAllowedForContext(
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin) const {
  // If FLoC is disabled completely, it is not available in any context.
  if (!IsFlocAllowed())
    return false;

  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxAllowedForContext(url, top_frame_origin,
                                           cookie_settings);
}

base::Time PrivacySandboxSettings::FlocDataAccessibleSince() const {
  return pref_service_->GetTime(prefs::kPrivacySandboxFlocDataAccessibleSince);
}

std::u16string PrivacySandboxSettings::GetFlocDescriptionForDisplay() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION,
      GetNumberOfDaysRoundedAboveOne(
          federated_learning::kFlocIdScheduledUpdateInterval.Get()));
}

std::u16string PrivacySandboxSettings::GetFlocIdForDisplay() const {
  const bool floc_feature_enabled = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortAPIOriginTrial);
  auto floc_id = federated_learning::FlocId::ReadFromPrefs(pref_service_);
  if (!IsFlocAllowed() || !floc_feature_enabled || !floc_id.IsValid())
    return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID);

  return base::NumberToString16(floc_id.ToUint64());
}

/*static*/ std::u16string PrivacySandboxSettings::GetFlocIdNextUpdateForDisplay(
    federated_learning::FlocIdProvider* floc_id_provider,
    PrefService* pref_service,
    const base::Time& current_time) {
  const bool floc_feature_enabled = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortAPIOriginTrial);

  if (!floc_id_provider || !floc_feature_enabled ||
      !IsFlocAllowedByPrefs(pref_service)) {
    return l10n_util::GetStringUTF16(
        IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE_INVALID);
  }

  auto next_compute_time = floc_id_provider->GetApproximateNextComputeTime();

  // There are no guarantee that the next compute time is in the future. This
  // should only occur when a compute is soon to occur, so assuming the current
  // time is suitable.
  if (next_compute_time < current_time)
    next_compute_time = current_time;

  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE,
      GetNumberOfDaysRoundedAboveOne(next_compute_time - current_time));
}

std::u16string PrivacySandboxSettings::GetFlocResetExplanationForDisplay()
    const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION,
      GetNumberOfDaysRoundedAboveOne(
          federated_learning::kFlocIdScheduledUpdateInterval.Get()));
}

std::u16string PrivacySandboxSettings::GetFlocStatusForDisplay() const {
  // FLoC always disabled while OT not active.
  // TODO(crbug.com/1287951): Perform cleanup / adjustment as required.
  return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE);
}

bool PrivacySandboxSettings::IsFlocIdResettable() const {
  const bool floc_feature_enabled = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortAPIOriginTrial);
  return floc_feature_enabled && IsFlocAllowed();
}

void PrivacySandboxSettings::ResetFlocId(bool user_initiated) const {
  SetFlocDataAccessibleFromNow(/*reset_calculate_timer=*/true);
  if (user_initiated) {
    base::RecordAction(
        base::UserMetricsAction("Settings.PrivacySandbox.ResetFloc"));
  }
}

bool PrivacySandboxSettings::IsFlocPrefEnabled() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxFlocEnabled);
}

void PrivacySandboxSettings::SetFlocPrefEnabled(bool enabled) const {
  pref_service_->SetBoolean(prefs::kPrivacySandboxFlocEnabled, enabled);
  base::RecordAction(base::UserMetricsAction(
      enabled ? "Settings.PrivacySandbox.FlocEnabled"
              : "Settings.PrivacySandbox.FlocDisabled"));
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
  // If the sandbox is disabled, then FLEDGE is never allowed.
  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return false;

  // Third party cookies must also be available for this context. An empty site
  // for cookies is provided so the context is always treated as a third party.
  return cookie_settings_->IsFullCookieAccessAllowed(
      auction_party, net::SiteForCookies(), top_frame_origin);
}

std::vector<GURL> PrivacySandboxSettings::FilterFledgeAllowedParties(
    const url::Origin& top_frame_origin,
    const std::vector<GURL>& auction_parties) {
  // If the sandbox is disabled, then no parties are allowed.
  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return {};

  std::vector<GURL> allowed_parties;
  for (const auto& party : auction_parties) {
    if (cookie_settings_->IsFullCookieAccessAllowed(
            party, net::SiteForCookies(), top_frame_origin)) {
      allowed_parties.push_back(party);
    }
  }
  return allowed_parties;
}

bool PrivacySandboxSettings::IsPrivacySandboxAllowed() {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

bool PrivacySandboxSettings::IsPrivacySandboxEnabled() {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

bool PrivacySandboxSettings::IsPrivacySandboxManaged() {
  return pref_service_->IsManagedPreference(prefs::kPrivacySandboxApisEnabled);
}

void PrivacySandboxSettings::SetPrivacySandboxEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxManuallyControlled, true);
  pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabled, enabled);
}

void PrivacySandboxSettings::OnCookiesCleared() {
  SetFlocDataAccessibleFromNow(/*reset_calculate_timer=*/false);
}

void PrivacySandboxSettings::OnPrivacySandboxPrefChanged() {
  // Any change of the two observed prefs should be accompanied by a
  // reset of the FLoC cohort. Technically this only needs to occur on the
  // transition from FLoC being effectively disabled to effectively enabled,
  // but performing it on every pref change achieves the same user visible
  // behavior, and is much simpler.
  ResetFlocId(/*user_initiated=*/false);
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
    const absl::optional<url::Origin>& top_frame_origin,
    const ContentSettingsForOneType& cookie_settings) const {
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
    sync_service_observer_.Observe(sync_service_.get());
  if (!identity_manager_observer_.IsObserving())
    identity_manager_observer_.Observe(identity_manager_.get());
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

void PrivacySandboxSettings::SetFlocDataAccessibleFromNow(
    bool reset_calculate_timer) const {
  pref_service_->SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince,
                         base::Time::Now());

  for (auto& observer : observers_)
    observer.OnFlocDataAccessibleSinceUpdated(reset_calculate_timer);
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
    const bool floc_enabled =
        pref_service_->GetBoolean(prefs::kPrivacySandboxFlocEnabled);

    if (default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
      RecordPrivacySandboxHistogram(
          floc_enabled ? PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                             kPSEnabledBlockAll
                       : PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                             kPSEnabledFlocDisabledBlockAll);
    } else if (cookie_controls_mode_value ==
               content_settings::CookieControlsMode::kBlockThirdParty) {
      RecordPrivacySandboxHistogram(
          floc_enabled ? PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                             kPSEnabledBlock3P
                       : PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                             kPSEnabledFlocDisabledBlock3P);
    } else {
      RecordPrivacySandboxHistogram(
          floc_enabled ? PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                             kPSEnabledAllowAll
                       : PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                             kPSEnabledFlocDisabledAllowAll);
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
