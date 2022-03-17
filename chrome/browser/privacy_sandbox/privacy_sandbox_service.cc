// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

#include <algorithm>
#include <iterator>

#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool g_dialog_diabled_for_tests = false;

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

PrivacySandboxService::PrivacySandboxService() = default;

PrivacySandboxService::PrivacySandboxService(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    content_settings::CookieSettings* cookie_settings,
    PrefService* pref_service,
    policy::PolicyService* policy_service,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    content::InterestGroupManager* interest_group_manager,
    profile_metrics::BrowserProfileType profile_type,
    content::BrowsingDataRemover* browsing_data_remover)
    : privacy_sandbox_settings_(privacy_sandbox_settings),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      policy_service_(policy_service),
      sync_service_(sync_service),
      identity_manager_(identity_manager),
      interest_group_manager_(interest_group_manager),
      profile_type_(profile_type),
      browsing_data_remover_(browsing_data_remover) {
  DCHECK(privacy_sandbox_settings_);
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);
  DCHECK(policy_service_);

  // Register observers for the Privacy Sandbox & FLoC preferences.
  user_prefs_registrar_.Init(pref_service_);
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxApisEnabled,
      base::BindRepeating(&PrivacySandboxService::OnPrivacySandboxV1PrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxApisEnabledV2,
      base::BindRepeating(&PrivacySandboxService::OnPrivacySandboxV2PrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxFlocEnabled,
      base::BindRepeating(&PrivacySandboxService::OnPrivacySandboxV1PrefChanged,
                          base::Unretained(this)));

  // On first entering the privacy sandbox experiment, users may have the
  // privacy sandbox disabled (or "reconciled") based on their current cookie
  // settings (e.g. blocking 3P cookies). Depending on the state of the sync
  // service, identity manager, and cookie setting, reconciliation may not run
  // immediately, or may not run at all.
  // TODO(crbug.com/1166665): Remove reconciliation logic when kAPI controls are
  // further separated from cookie controls.
  MaybeReconcilePrivacySandboxPref();

  // If the Sandbox is currently restricted, disable the V2 preference. The user
  // must manually enable the sandbox if they stop being restricted.
  if (IsPrivacySandboxRestricted())
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
}

PrivacySandboxService::~PrivacySandboxService() = default;

PrivacySandboxService::DialogType
PrivacySandboxService::GetRequiredDialogType() {
  const auto cookie_controls_mode =
      static_cast<content_settings::CookieControlsMode>(
          pref_service_->GetInteger(prefs::kCookieControlsMode));
  const auto default_content_setting =
      cookie_settings_->GetDefaultCookieSetting(/*provider_id=*/nullptr);
  const auto third_party_cookies_blocked =
      default_content_setting == ContentSetting::CONTENT_SETTING_BLOCK ||
      cookie_controls_mode ==
          content_settings::CookieControlsMode::kBlockThirdParty;
  return GetRequiredDialogTypeInternal(pref_service_, profile_type_,
                                       privacy_sandbox_settings_,
                                       third_party_cookies_blocked);
}

void PrivacySandboxService::DialogActionOccurred(
    PrivacySandboxService::DialogAction action) {
  switch (action) {
    case (DialogAction::kNoticeShown): {
      DCHECK_EQ(DialogType::kNotice, GetRequiredDialogType());
      // The new Privacy Sandbox pref can be enabled when the notice has been
      // shown. Note that a notice will not have been shown if the user disabled
      // the old Privacy Sandbox pref.
      pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxNoticeDisplayed, true);
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.Shown"));
      break;
    }
    case (DialogAction::kNoticeOpenSettings): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.OpenedSettings"));
      break;
    }
    case (DialogAction::kNoticeAcknowledge): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.Acknowledged"));
      break;
    }
    case (DialogAction::kNoticeDismiss): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.Dismissed"));
      break;
    }
    case (DialogAction::kNoticeClosedNoInteraction): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.ClosedNoInteraction"));
      break;
    }
    case (DialogAction::kConsentShown): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Shown"));
      break;
    }
    case (DialogAction::kConsentAccepted): {
      pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxConsentDecisionMade,
                                true);
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Accepted"));
      break;
    }
    case (DialogAction::kConsentDeclined): {
      pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
      pref_service_->SetBoolean(prefs::kPrivacySandboxConsentDecisionMade,
                                true);
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Declined"));
      break;
    }
    case (DialogAction::kConsentMoreInfoOpened): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.LearnMoreExpanded"));
      break;
    }
    case (DialogAction::kConsentClosedNoDecision): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.ClosedNoInteraction"));
      break;
    }
    default:
      break;
  }
}

// static
bool PrivacySandboxService::IsUrlSuitableForDialog(const GURL& url) {
  // about:blank is valid.
  if (url.IsAboutBlank())
    return true;

  // Other than about:blank, only chrome:// urls are valid. This check is early
  // in processing to immediately exclude most URLs.
  if (!url.SchemeIs(content::kChromeUIScheme))
    return false;

  // The welcome page is never valid.
  if (url.host() == chrome::kChromeUIWelcomeHost)
    return false;

  // The generic new tab is never valid, only NTPs known to be Chrome controlled
  // are valid.
  if (url.host() == chrome::kChromeUINewTabHost)
    return false;

  // All remaining chrome:// pages are considered valid.
  return true;
}

void PrivacySandboxService::DialogOpenedForBrowser(Browser* browser) {
  DCHECK(!browsers_with_open_dialogs_.count(browser));
  browsers_with_open_dialogs_.insert(browser);
}

void PrivacySandboxService::DialogClosedForBrowser(Browser* browser) {
  DCHECK(browsers_with_open_dialogs_.count(browser));
  browsers_with_open_dialogs_.erase(browser);
}

bool PrivacySandboxService::IsDialogOpenForBrowser(Browser* browser) {
  return browsers_with_open_dialogs_.count(browser);
}

void PrivacySandboxService::SetDialogDisabledForTests(bool disabled) {
  g_dialog_diabled_for_tests = disabled;
}

std::u16string PrivacySandboxService::GetFlocDescriptionForDisplay() const {
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION,
      GetNumberOfDaysRoundedAboveOne(base::Days(7)));
}

std::u16string PrivacySandboxService::GetFlocIdForDisplay() const {
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID);
}

std::u16string PrivacySandboxService::GetFlocIdNextUpdateForDisplay(
    const base::Time& current_time) {
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return l10n_util::GetStringUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE_INVALID);
}

std::u16string PrivacySandboxService::GetFlocResetExplanationForDisplay()
    const {
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return l10n_util::GetPluralStringFUTF16(
      IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION,
      GetNumberOfDaysRoundedAboveOne(base::Days(7)));
}

std::u16string PrivacySandboxService::GetFlocStatusForDisplay() const {
  // FLoC always disabled while OT not active.
  // TODO(crbug.com/1299720): Perform cleanup / adjustment as required.
  return l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE);
}

bool PrivacySandboxService::IsFlocIdResettable() const {
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return false;
}

void PrivacySandboxService::ResetFlocId(bool user_initiated) const {
  // This function is left as a non-functional stub to support UI code for the
  // removed FLoC feature. The UI should not allow the user to perform this
  // action (see IsFlocIdResettable() definition)
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return;
}

bool PrivacySandboxService::IsFlocPrefEnabled() const {
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return false;
}

void PrivacySandboxService::SetFlocPrefEnabled(bool enabled) const {
  // TODO(crbug.com/1299720): Remove this and all the UI code which uses it.
  return;
}

bool PrivacySandboxService::IsPrivacySandboxEnabled() {
  return privacy_sandbox_settings_->IsPrivacySandboxEnabled();
}

bool PrivacySandboxService::IsPrivacySandboxManaged() {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxSettings3)) {
    return pref_service_->IsManagedPreference(
        prefs::kPrivacySandboxApisEnabled);
  }
  return pref_service_->IsManagedPreference(
      prefs::kPrivacySandboxApisEnabledV2);
}

bool PrivacySandboxService::IsPrivacySandboxRestricted() {
  return privacy_sandbox_settings_->IsPrivacySandboxRestricted();
}

void PrivacySandboxService::SetPrivacySandboxEnabled(bool enabled) {
  privacy_sandbox_settings_->SetPrivacySandboxEnabled(enabled);
}

void PrivacySandboxService::OnPrivacySandboxV1PrefChanged() {
  // Any change of the two observed prefs should be accompanied by a
  // reset of the FLoC cohort. Technically this only needs to occur on the
  // transition from FLoC being effectively disabled to effectively enabled,
  // but performing it on every pref change achieves the same user visible
  // behavior, and is much simpler.
  ResetFlocId(/*user_initiated=*/false);
}

void PrivacySandboxService::OnPrivacySandboxV2PrefChanged() {
  // If the user has disabled the Privacy Sanbdbox, any data stored should be
  // cleared.
  if (!browsing_data_remover_)
    return;

  if (pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabledV2))
    return;

  browsing_data_remover_->Remove(
      base::Time::Min(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS |
          content::BrowsingDataRemover::DATA_TYPE_AGGREGATION_SERVICE |
          content::BrowsingDataRemover::DATA_TYPE_CONVERSIONS |
          content::BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
}

void PrivacySandboxService::GetFledgeJoiningEtldPlusOneForDisplay(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  if (!interest_group_manager_) {
    std::move(callback).Run({});
    return;
  }

  interest_group_manager_->GetAllInterestGroupJoiningOrigins(base::BindOnce(
      &PrivacySandboxService::ConvertFledgeJoiningTopFramesForDisplay,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<std::string>
PrivacySandboxService::GetBlockedFledgeJoiningTopFramesForDisplay() const {
  auto* pref_value =
      pref_service_->GetDictionary(prefs::kPrivacySandboxFledgeJoinBlocked);
  DCHECK(pref_value->is_dict());

  std::vector<std::string> blocked_top_frames;

  for (auto entry : pref_value->DictItems())
    blocked_top_frames.emplace_back(entry.first);

  // Apply a lexographic ordering to match other settings permission surfaces.
  std::sort(blocked_top_frames.begin(), blocked_top_frames.end());

  return blocked_top_frames;
}

void PrivacySandboxService::SetFledgeJoiningAllowed(
    const std::string& top_frame_etld_plus1,
    bool allowed) const {
  privacy_sandbox_settings_->SetFledgeJoiningAllowed(top_frame_etld_plus1,
                                                     allowed);

  if (!allowed && browsing_data_remover_) {
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter =
        content::BrowsingDataFilterBuilder::Create(
            content::BrowsingDataFilterBuilder::Mode::kDelete);
    filter->AddRegisterableDomain(top_frame_etld_plus1);
    browsing_data_remover_->RemoveWithFilter(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter));
  }
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
  // Reconciliation only ever affects the synced, pre-notice / consent pref.
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

void PrivacySandboxService::RecordPrivacySandbox3StartupMetrics() {
  const std::string privacy_sandbox_startup_histogram =
      "Settings.PrivacySandbox.StartupState";
  const bool sandbox_v2_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabledV2);

  // Handle PS V1 prefs disabled.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxDisabled)) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_startup_histogram,
        sandbox_v2_enabled ? PSStartupStates::kDialogOffV1OffEnabled
                           : PSStartupStates::kDialogOffV1OffDisabled);
    return;
  }
  // Handle 3PC disabled.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked)) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_startup_histogram,
        sandbox_v2_enabled ? PSStartupStates::kDialogOff3PCOffEnabled
                           : PSStartupStates::kDialogOff3PCOffDisabled);
    return;
  }
  // Handle managed.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxManaged)) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_startup_histogram,
        sandbox_v2_enabled ? PSStartupStates::kDialogOffManagedEnabled
                           : PSStartupStates::kDialogOffManagedDisabled);
    return;
  }
  // Handle restricted.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxRestricted)) {
    base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                  PSStartupStates::kDialogOffRestricted);
    return;
  }
  if (privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get()) {
    if (!pref_service_->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade)) {
      base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                    PSStartupStates::kDialogWaiting);
      return;
    }
    base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                  sandbox_v2_enabled
                                      ? PSStartupStates::kConsentShownEnabled
                                      : PSStartupStates::kConsentShownDisabled);
  } else {  // Notice required.
    if (!pref_service_->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed)) {
      base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                    PSStartupStates::kDialogWaiting);
      return;
    }
    base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                  sandbox_v2_enabled
                                      ? PSStartupStates::kNoticeShownEnabled
                                      : PSStartupStates::kNoticeShownDisabled);
  }
}

void PrivacySandboxService::LogPrivacySandboxState() {
  // Do not record metrics for non-regular profiles.
  if (profile_type_ != profile_metrics::BrowserProfileType::kRegular)
    return;

  // Start by recording any metrics for Privacy Sandbox 3.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3)) {
    RecordPrivacySandbox3StartupMetrics();
  }

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

  if (privacy_sandbox_settings_->IsPrivacySandboxEnabled()) {
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

void PrivacySandboxService::ConvertFledgeJoiningTopFramesForDisplay(
    base::OnceCallback<void(std::vector<std::string>)> callback,
    std::vector<url::Origin> top_frames) {
  std::set<std::string> display_entries;
  for (const auto& origin : top_frames) {
    // Prefer to display the associated eTLD+1, if there is one.
    auto etld_plus_one = net::registry_controlled_domains::GetDomainAndRegistry(
        origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (etld_plus_one.length() > 0) {
      display_entries.emplace(std::move(etld_plus_one));
      continue;
    }

    // The next best option is a host, which may be an IP address or an eTLD
    // itself (e.g. github.io).
    if (origin.host().length() > 0) {
      display_entries.emplace(origin.host());
      continue;
    }

    // Other types of top-frame origins (file, opaque) do not support FLEDGE.
    NOTREACHED();
  }
  // TODO(crbug.com/1286276): Enforce a friendlier ordering instead of just
  // whatever the database gives back.
  std::move(callback).Run(
      std::vector<std::string>{display_entries.begin(), display_entries.end()});
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxService::GetCurrentTopTopics() const {
  // TODO(crbug.com/1286276): Add proper Topics implementation.
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get())
    return {fake_current_topics_.begin(), fake_current_topics_.end()};
  return {};
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxService::GetBlockedTopics() const {
  // TODO(crbug.com/1286276): Add proper Topics implementation.
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get())
    return {fake_blocked_topics_.begin(), fake_blocked_topics_.end()};
  return {};
}

void PrivacySandboxService::SetTopicAllowed(
    privacy_sandbox::CanonicalTopic topic,
    bool allowed) {
  // TODO(crbug.com/1286276): Update preferences.
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get()) {
    if (allowed) {
      fake_current_topics_.insert(topic);
      fake_blocked_topics_.erase(topic);
    } else {
      fake_current_topics_.erase(topic);
      fake_blocked_topics_.insert(topic);
    }
  }
}

/*static*/ PrivacySandboxService::DialogType
PrivacySandboxService::GetRequiredDialogTypeInternal(
    PrefService* pref_service,
    profile_metrics::BrowserProfileType profile_type,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    bool third_party_cookies_blocked) {
  // If the dialog is disabled for testing, never show it.
  if (g_dialog_diabled_for_tests)
    return DialogType::kNone;

  // If the profile isn't a regular profile, no dialog should ever be shown.
  if (profile_type != profile_metrics::BrowserProfileType::kRegular)
    return DialogType::kNone;

  // If the release 3 feature is not enabled, no dialog is required.
  if (!base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3))
    return DialogType::kNone;

  // Forced testing feature parameters override everything.
  if (privacy_sandbox::kPrivacySandboxSettings3DisableDialogForTesting.Get())
    return DialogType::kNone;

  if (privacy_sandbox::kPrivacySandboxSettings3ForceShowConsentForTesting.Get())
    return DialogType::kConsent;

  if (privacy_sandbox::kPrivacySandboxSettings3ForceShowNoticeForTesting.Get())
    return DialogType::kNotice;

  // Start by checking for any previous decision about the dialog, such as
  // it already having been shown, or not having been shown for some reason.
  // These checks for previous decisions occur in advance of their corresponding
  // decisions later in this function, so that changes to profile state to not
  // appear to impact previous decisions.

  // If a user wasn't shown a confirmation because they previously turned the
  // Privacy Sandbox off, we do not attempt to re-show one.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxDisabled)) {
    return DialogType::kNone;
  }

  // If a consent decision has already been made, no dialog is required.
  if (pref_service->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade))
    return DialogType::kNone;

  // If only a notice is required, and has been shown, no dialog is required.
  if (!privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get() &&
      pref_service->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed)) {
    return DialogType::kNone;
  }

  // If a user wasn't shown a confirmation because the sandbox was previously
  // restricted, do not attempt to show them one. The user will be able to
  // enable the Sandbox on the settings page.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxRestricted)) {
    return DialogType::kNone;
  }

  // If a user wasn't shown a dialog previously because the Privacy Sandbox
  // was managed, do not show them one.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxManaged)) {
    return DialogType::kNone;
  }

  // If a user wasn't shown a confirmation because they block third party
  // cookies, we do not attempt to re-show one.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked)) {
    return DialogType::kNone;
  }

  // If the Privacy Sandbox is restricted, no dialog is shown.
  if (privacy_sandbox_settings->IsPrivacySandboxRestricted()) {
    pref_service->SetBoolean(
        prefs::kPrivacySandboxNoConfirmationSandboxRestricted, true);
    return DialogType::kNone;
  }

  // If the Privacy Sandbox is managed, no dialog is shown.
  if (pref_service->FindPreference(prefs::kPrivacySandboxApisEnabledV2)
          ->IsManaged()) {
    pref_service->SetBoolean(prefs::kPrivacySandboxNoConfirmationSandboxManaged,
                             true);
    return DialogType::kNone;
  }

  // If the user blocks third party cookies, then no dialog is shown.
  if (third_party_cookies_blocked) {
    pref_service->SetBoolean(
        prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked, true);
    return DialogType::kNone;
  }

  // If a user now requires consent, but has previously seen a notice, whether
  // a consent is shown depends on their current Privacy Sandbox setting.
  if (privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get() &&
      pref_service->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed)) {
    DCHECK(
        !pref_service->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade));

    // As the user has not yet consented, the V2 pref must be disabled.
    // However, this may not be the first time that this function is being
    // called. The API for this service guarantees, and clients depend, on
    // successive calls to this function returning the same value. Browser
    // restarts & updates via DialogActionOccurred() notwithstanding. To achieve
    // this, we need to distinguish between the case where the user themselves
    // previously disabled the APIs, and when this logic disabled them
    // previously due to having insufficient confirmation.
    if (pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabledV2)) {
      pref_service->SetBoolean(
          prefs::kPrivacySandboxDisabledInsufficientConfirmation, true);
      pref_service->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
    }

    if (pref_service->GetBoolean(
            prefs::kPrivacySandboxDisabledInsufficientConfirmation)) {
      return DialogType::kConsent;
    } else {
      DCHECK(!pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
      pref_service->SetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxDisabled, true);
      return DialogType::kNone;
    }
  }

  // At this point, no previous decision should have been made.
  DCHECK(!pref_service->GetBoolean(
      prefs::kPrivacySandboxNoConfirmationSandboxDisabled));
  DCHECK(!pref_service->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed));
  DCHECK(!pref_service->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade));

  // The user should not have been able to enable the Sandbox without a
  // previous decision having been made. The exception to this is through test
  // only feature parameters, which will have let the user skip confirmation.
  DCHECK(!pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));

  // If the user had previously disabled the Privacy Sandbox, no confirmation
  // will be shown.
  if (!pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabled)) {
    pref_service->SetBoolean(
        prefs::kPrivacySandboxNoConfirmationSandboxDisabled, true);
    return DialogType::kNone;
  }

  // Check if the users requires a consent. This information is provided by
  // feature parameter to allow Finch based geo-targeting.
  if (privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get())
    return DialogType::kConsent;

  // Finally a notice is required.
  return DialogType::kNotice;
}
