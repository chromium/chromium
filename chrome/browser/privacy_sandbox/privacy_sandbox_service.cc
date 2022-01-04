// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

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
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

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

PrivacySandboxService::PrivacySandboxService(
    PrivacySandboxSettings* privacy_sandbox_settings,
    content_settings::CookieSettings* cookie_settings,
    PrefService* pref_service,
    policy::PolicyService* policy_service,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    federated_learning::FlocIdProvider* floc_id_provider)
    : privacy_sandbox_settings_(privacy_sandbox_settings),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      policy_service_(policy_service),
      sync_service_(sync_service),
      identity_manager_(identity_manager),
      floc_id_provider_(floc_id_provider) {
  DCHECK(privacy_sandbox_settings_);
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);
  DCHECK(policy_service_);

  // Register observers for the Privacy Sandbox & FLoC preferences.
  user_prefs_registrar_.Init(pref_service_);
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxApisEnabled,
      base::BindRepeating(&PrivacySandboxService::OnPrivacySandboxPrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxFlocEnabled,
      base::BindRepeating(&PrivacySandboxService::OnPrivacySandboxPrefChanged,
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

PrivacySandboxService::~PrivacySandboxService() = default;

std::u16string PrivacySandboxService::GetFlocDescriptionForDisplay() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION,
      GetNumberOfDaysRoundedAboveOne(
          federated_learning::kFlocIdScheduledUpdateInterval.Get()));
}

std::u16string PrivacySandboxService::GetFlocIdForDisplay() const {
  const bool floc_feature_enabled = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortAPIOriginTrial);
  auto floc_id = federated_learning::FlocId::ReadFromPrefs(pref_service_);

  if (!privacy_sandbox_settings_->IsFlocAllowed() || !floc_feature_enabled ||
      !floc_id.IsValid())
    return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID);

  return base::NumberToString16(floc_id.ToUint64());
}

std::u16string PrivacySandboxService::GetFlocIdNextUpdateForDisplay(
    const base::Time& current_time) {
  const bool floc_feature_enabled = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortAPIOriginTrial);

  if (!floc_id_provider_ || !floc_feature_enabled ||
      !privacy_sandbox_settings_->IsFlocAllowed()) {
    return l10n_util::GetStringUTF16(
        IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE_INVALID);
  }

  auto next_compute_time = floc_id_provider_->GetApproximateNextComputeTime();

  // There are no guarantee that the next compute time is in the future. This
  // should only occur when a compute is soon to occur, so assuming the current
  // time is suitable.
  if (next_compute_time < current_time)
    next_compute_time = current_time;

  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE,
      GetNumberOfDaysRoundedAboveOne(next_compute_time - current_time));
}

std::u16string PrivacySandboxService::GetFlocResetExplanationForDisplay()
    const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION,
      GetNumberOfDaysRoundedAboveOne(
          federated_learning::kFlocIdScheduledUpdateInterval.Get()));
}

std::u16string PrivacySandboxService::GetFlocStatusForDisplay() const {
  const bool floc_feature_enabled = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortAPIOriginTrial);
  const bool floc_setting_enabled = privacy_sandbox_settings_->IsFlocAllowed();
  if (floc_setting_enabled) {
    return floc_feature_enabled
               ? l10n_util::GetStringUTF16(
                     IDS_PRIVACY_SANDBOX_FLOC_STATUS_ACTIVE)
               : l10n_util::GetStringUTF16(
                     IDS_PRIVACY_SANDBOX_FLOC_STATUS_ELIGIBLE_NOT_ACTIVE);
  }

  return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE);
}

bool PrivacySandboxService::IsFlocIdResettable() const {
  const bool floc_feature_enabled = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortAPIOriginTrial);
  return floc_feature_enabled && privacy_sandbox_settings_->IsFlocAllowed();
}

void PrivacySandboxService::ResetFlocId(bool user_initiated) const {
  privacy_sandbox_settings_->SetFlocDataAccessibleFromNow(
      /*reset_calculate_timer=*/true);
  if (user_initiated) {
    base::RecordAction(
        base::UserMetricsAction("Settings.PrivacySandbox.ResetFloc"));
  }
}

bool PrivacySandboxService::IsFlocPrefEnabled() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxFlocEnabled);
}

void PrivacySandboxService::SetFlocPrefEnabled(bool enabled) const {
  pref_service_->SetBoolean(prefs::kPrivacySandboxFlocEnabled, enabled);
  base::RecordAction(base::UserMetricsAction(
      enabled ? "Settings.PrivacySandbox.FlocEnabled"
              : "Settings.PrivacySandbox.FlocDisabled"));
}

bool PrivacySandboxService::IsPrivacySandboxEnabled() {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

bool PrivacySandboxService::IsPrivacySandboxManaged() {
  return pref_service_->IsManagedPreference(prefs::kPrivacySandboxApisEnabled);
}

void PrivacySandboxService::OnPrivacySandboxPrefChanged() {
  // Any change of the two observed prefs should be accompanied by a
  // reset of the FLoC cohort. Technically this only needs to occur on the
  // transition from FLoC being effectively disabled to effectively enabled,
  // but performing it on every pref change achieves the same user visible
  // behavior, and is much simpler.
  ResetFlocId(/*user_initiated=*/false);
}

void PrivacySandboxService::Shutdown() {
  StopObserving();
}

void PrivacySandboxService::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                            const policy::PolicyMap& previous,
                                            const policy::PolicyMap& current) {
  // |pref_service_| and |cookie_settings_| will have been made aware
  // of the policy changes before this observer function is called.
  MaybeReconcilePrivacySandboxPref();
}

void PrivacySandboxService::OnStateChanged(syncer::SyncService* sync) {
  MaybeReconcilePrivacySandboxPref();
}

void PrivacySandboxService::OnSyncCycleCompleted(syncer::SyncService* sync) {
  MaybeReconcilePrivacySandboxPref();
}

void PrivacySandboxService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  MaybeReconcilePrivacySandboxPref();
}

void PrivacySandboxService::MaybeReconcilePrivacySandboxPref() {
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
  DCHECK(identity_manager_);
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

void PrivacySandboxService::ReconcilePrivacySandboxPref() {
  if (ShouldDisablePrivacySandbox(cookie_settings_, pref_service_))
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);

  pref_service_->SetBoolean(prefs::kPrivacySandboxPreferencesReconciled, true);

  // If observers were setup they are no longer required after reconciliation
  // has occurred.
  StopObserving();
  LogPrivacySandboxState();
}

void PrivacySandboxService::StopObserving() {
  // Removing a non-observing observer is a no-op.
  sync_service_observer_.Reset();
  identity_manager_observer_.Reset();
  if (policy_service_observed_) {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    policy_service_observed_ = false;
  }
}

void PrivacySandboxService::RecordPrivacySandboxHistogram(
    PrivacySandboxService::SettingsPrivacySandboxEnabled state) {
  base::UmaHistogramEnumeration("Settings.PrivacySandbox.Enabled", state);
}

void PrivacySandboxService::LogPrivacySandboxState() {
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
        PrivacySandboxService::SettingsPrivacySandboxEnabled::
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
        PrivacySandboxService::SettingsPrivacySandboxEnabled::
            kPSDisabledPolicyBlock3P);
    return;
  }

  if (pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled)) {
    const bool floc_enabled =
        pref_service_->GetBoolean(prefs::kPrivacySandboxFlocEnabled);

    if (default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
      RecordPrivacySandboxHistogram(
          floc_enabled ? PrivacySandboxService::SettingsPrivacySandboxEnabled::
                             kPSEnabledBlockAll
                       : PrivacySandboxService::SettingsPrivacySandboxEnabled::
                             kPSEnabledFlocDisabledBlockAll);
    } else if (cookie_controls_mode_value ==
               content_settings::CookieControlsMode::kBlockThirdParty) {
      RecordPrivacySandboxHistogram(
          floc_enabled ? PrivacySandboxService::SettingsPrivacySandboxEnabled::
                             kPSEnabledBlock3P
                       : PrivacySandboxService::SettingsPrivacySandboxEnabled::
                             kPSEnabledFlocDisabledBlock3P);
    } else {
      RecordPrivacySandboxHistogram(
          floc_enabled ? PrivacySandboxService::SettingsPrivacySandboxEnabled::
                             kPSEnabledAllowAll
                       : PrivacySandboxService::SettingsPrivacySandboxEnabled::
                             kPSEnabledFlocDisabledAllowAll);
    }
  } else {
    if (default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxService::SettingsPrivacySandboxEnabled::
              kPSDisabledBlockAll);
    } else if (cookie_controls_mode_value ==
               content_settings::CookieControlsMode::kBlockThirdParty) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxService::SettingsPrivacySandboxEnabled::
              kPSDisabledBlock3P);
    } else {
      RecordPrivacySandboxHistogram(
          PrivacySandboxService::SettingsPrivacySandboxEnabled::
              kPSDisabledAllowAll);
    }
  }
}
