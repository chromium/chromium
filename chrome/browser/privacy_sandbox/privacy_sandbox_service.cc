// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

#include <algorithm>
#include <iterator>
#include <numeric>

#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#endif

namespace {

constexpr char kBlockedTopicsTopicKey[] = "topic";

bool g_prompt_disabled_for_tests = false;

// Returns whether 3P cookies are blocked by |cookie_settings|. This can be
// either through blocking 3P cookies directly, or blocking all cookies.
bool AreThirdPartyCookiesBlocked(
    content_settings::CookieSettings* cookie_settings) {
  const auto default_content_setting =
      cookie_settings->GetDefaultCookieSetting(/*provider_id=*/nullptr);
  return cookie_settings->ShouldBlockThirdPartyCookies() ||
         default_content_setting == ContentSetting::CONTENT_SETTING_BLOCK;
}

// Sorts |topics| alphabetically by topic display name for display.
void SortTopicsForDisplay(
    std::vector<privacy_sandbox::CanonicalTopic>& topics) {
  std::sort(topics.begin(), topics.end(),
            [](const privacy_sandbox::CanonicalTopic& a,
               const privacy_sandbox::CanonicalTopic& b) {
              return a.GetLocalizedRepresentation() <
                     b.GetLocalizedRepresentation();
            });
}

// Returns whether |profile_type|, and the current browser session on CrOS,
// represent a regular (e.g. non guest, non system, non kiosk) profile.
bool IsRegularProfile(profile_metrics::BrowserProfileType profile_type) {
  if (profile_type != profile_metrics::BrowserProfileType::kRegular) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Any Device Local account, which is a CrOS concept powering things like
  // Kiosks and Managed Guest Sessions, is not considered regular.
  return !profiles::IsPublicSession() && !profiles::IsKioskSession() &&
         !profiles::IsChromeAppKioskSession();
#else
  return true;
#endif
}

// Returns the text contents of the Topics Consent dialog.
std::string GetTopicsConfirmationText() {
  std::vector<int> string_ids = {
      IDS_PRIVACY_SANDBOX_M1_CONSENT_TITLE,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_1,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_2,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_3,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_4,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_EXPAND_LABEL,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_1,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_2,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_3,
      IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_LINK};

  return std::accumulate(
      string_ids.begin(), string_ids.end(), std::string(),
      [](const std::string& previous_result, int next_id) {
        auto next_string = l10n_util::GetStringUTF8(next_id);
        // Remove bold tags present in some strings.
        base::ReplaceSubstringsAfterOffset(&next_string, 0, "<b>", "");
        base::ReplaceSubstringsAfterOffset(&next_string, 0, "</b>", "");
        return previous_result + (!previous_result.empty() ? " " : "") +
               next_string;
      }

  );
}

// Returns the text contents of the Topics settings page.
std::string GetTopicsSettingsText(bool did_consent,
                                  bool has_current_topics,
                                  bool has_blocked_topics) {
  // `did_consent` refers to the _updated_ state, and so the previous state,
  // e.g. when the user clicked the toggle, will be the opposite.
  auto topics_prev_enabled = !did_consent;

  // A user should only have current topics while topics is enabled. Old topics
  // will not appear when the user enables, as they will have been cleared when
  // topics was previously disabled, or never generated at all.
  DCHECK(topics_prev_enabled || !has_current_topics);

  int blocked_topics_description =
      has_blocked_topics
          ? IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION
          : IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY;

  std::vector<int> string_ids = {
      IDS_SETTINGS_TOPICS_PAGE_TITLE,
      IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
      IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
      IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
      IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
      IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
      IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
      IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
      IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3,
      IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
      blocked_topics_description,
      IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};

  // Additional strings are displayed if there were no current topics, either
  // because they were empty, or because Topics was disabled. These will have
  // appeared after the current topics description.
  if (!topics_prev_enabled) {
    string_ids.insert(
        string_ids.begin() + 5,
        IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED);
  } else if (!has_current_topics) {
    string_ids.insert(
        string_ids.begin() + 5,
        IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY);
  }

  return std::accumulate(string_ids.begin(), string_ids.end(), std::string(),
                         [](const std::string& previous_result, int next_id) {
                           auto next_string = l10n_util::GetStringUTF8(next_id);
                           return previous_result +
                                  (!previous_result.empty() ? " " : "") +
                                  next_string;
                         });
}

// Returns whether this is a Google Chrome-branded build.
bool IsChromeBuild() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

}  // namespace

PrivacySandboxService::PrivacySandboxService() = default;

PrivacySandboxService::PrivacySandboxService(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    content_settings::CookieSettings* cookie_settings,
    PrefService* pref_service,
    content::InterestGroupManager* interest_group_manager,
    profile_metrics::BrowserProfileType profile_type,
    content::BrowsingDataRemover* browsing_data_remover,
    HostContentSettingsMap* host_content_settings_map,
#if !BUILDFLAG(IS_ANDROID)
    TrustSafetySentimentService* sentiment_service,
#endif
    browsing_topics::BrowsingTopicsService* browsing_topics_service,
    first_party_sets::FirstPartySetsPolicyService* first_party_sets_service)
    : privacy_sandbox_settings_(privacy_sandbox_settings),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      interest_group_manager_(interest_group_manager),
      profile_type_(profile_type),
      browsing_data_remover_(browsing_data_remover),
      host_content_settings_map_(host_content_settings_map),
#if !BUILDFLAG(IS_ANDROID)
      sentiment_service_(sentiment_service),
#endif
      browsing_topics_service_(browsing_topics_service),
      first_party_sets_policy_service_(first_party_sets_service) {
  DCHECK(privacy_sandbox_settings_);
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);

  // Register observers for the Privacy Sandbox preferences.
  user_prefs_registrar_.Init(pref_service_);
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxApisEnabledV2,
      base::BindRepeating(&PrivacySandboxService::OnPrivacySandboxV2PrefChanged,
                          base::Unretained(this)));

  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1TopicsEnabled,
      base::BindRepeating(&PrivacySandboxService::OnTopicsPrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1FledgeEnabled,
      base::BindRepeating(&PrivacySandboxService::OnFledgePrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1AdMeasurementEnabled,
      base::BindRepeating(&PrivacySandboxService::OnAdMeasurementPrefChanged,
                          base::Unretained(this)));

  // If the Sandbox is currently restricted, disable it and reset any consent
  // information. The user must manually enable the sandbox if they stop being
  // restricted.
  if (IsPrivacySandboxRestricted()) {
    // Disable trials prefs.
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);

    // Disable M1 prefs.
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                              false);

    // Clear any recorded consent information.
    pref_service_->ClearPref(prefs::kPrivacySandboxTopicsConsentGiven);
    pref_service_->ClearPref(prefs::kPrivacySandboxTopicsConsentLastUpdateTime);
    pref_service_->ClearPref(
        prefs::kPrivacySandboxTopicsConsentLastUpdateReason);
    pref_service_->ClearPref(
        prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate);
  }

  // Check for FPS pref init at each startup.
  // TODO(crbug.com/1351327): Remove this logic when most users have run init.
  MaybeInitializeFirstPartySetsPref();

  // Check for anti-abuse content setting init at each startup.
  // TODO(crbug.com/1408778): Remove this logic when most users have run init.
  MaybeInitializeAntiAbuseContentSetting();

  // Record preference state for UMA at each startup.
  LogPrivacySandboxState();
}

PrivacySandboxService::~PrivacySandboxService() = default;

PrivacySandboxService::PromptType
PrivacySandboxService::GetRequiredPromptType() {
  const auto third_party_cookies_blocked =
      AreThirdPartyCookiesBlocked(cookie_settings_);
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    return GetRequiredPromptTypeInternalM1(
        pref_service_, profile_type_, privacy_sandbox_settings_,
        third_party_cookies_blocked,
        force_chrome_build_for_tests_ || IsChromeBuild());
  }
  return GetRequiredPromptTypeInternal(pref_service_, profile_type_,
                                       privacy_sandbox_settings_,
                                       third_party_cookies_blocked);
}

void PrivacySandboxService::PromptActionOccurred(
    PrivacySandboxService::PromptAction action) {
  RecordPromptActionMetrics(action);

  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    PromptActionOccurredM1(action);
    return;
  }

  InformSentimentService(action);
  if (PromptAction::kNoticeShown == action &&
      PromptType::kNotice == GetRequiredPromptType()) {
    // The Privacy Sandbox pref can be enabled when the notice has been
    // shown. Note that a notice will not have been shown if the user
    // disabled the old Privacy Sandbox pref.
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxNoticeDisplayed, true);
  } else if (PromptAction::kConsentAccepted == action) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxConsentDecisionMade, true);
  } else if (PromptAction::kConsentDeclined == action) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
    pref_service_->SetBoolean(prefs::kPrivacySandboxConsentDecisionMade, true);
  }
}

void PrivacySandboxService::PromptActionOccurredM1(
    PrivacySandboxService::PromptAction action) {
  DCHECK(
      base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4));

  InformSentimentServiceM1(action);
  if (PromptAction::kNoticeAcknowledge == action ||
      PromptAction::kNoticeOpenSettings == action) {
    if (privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get()) {
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged,
                                true);
      // It's possible the user is seeing this notice as part of an upgrade to
      // EEA consent. In this instance, we shouldn't alter the control state,
      // as the user may have already altered it in settings.
      if (!pref_service_->GetBoolean(
              prefs::kPrivacySandboxM1RowNoticeAcknowledged)) {
        pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
        pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                  true);
      }
    } else {
      DCHECK(privacy_sandbox::kPrivacySandboxSettings4NoticeRequired.Get());
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged,
                                true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                true);
    }
  } else if (PromptAction::kConsentAccepted == action) {
    DCHECK(privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get());
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade,
                              true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
    RecordUpdatedTopicsConsent(
        privacy_sandbox::TopicsConsentUpdateSource::kConfirmation, true);
  } else if (PromptAction::kConsentDeclined == action) {
    DCHECK(privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get());
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade,
                              true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    RecordUpdatedTopicsConsent(
        privacy_sandbox::TopicsConsentUpdateSource::kConfirmation, false);
  }
}

// static
bool PrivacySandboxService::IsUrlSuitableForPrompt(const GURL& url) {
  // The prompt should be shown on a limited list of pages:

  // about:blank is valid.
  if (url.IsAboutBlank()) {
    return true;
  }
  // Chrome settings page is valid. The subpages aren't as most of them are not
  // related to the prompt.
  if (url == GURL(chrome::kChromeUISettingsURL)) {
    return true;
  }
  // Chrome history is valid as the prompt mentions history.
  if (url == GURL(chrome::kChromeUIHistoryURL)) {
    return true;
  }
  // Only a Chrome controlled New Tab Page is valid. Third party NTP is still
  // Chrome controlled, but is without Google branding.
  if (url == GURL(chrome::kChromeUINewTabPageURL) ||
      url == GURL(chrome::kChromeUINewTabPageThirdPartyURL)) {
    return true;
  }

  return false;
}

void PrivacySandboxService::PromptOpenedForBrowser(Browser* browser) {
  DCHECK(!browsers_with_open_prompts_.count(browser));
  browsers_with_open_prompts_.insert(browser);
}

void PrivacySandboxService::PromptClosedForBrowser(Browser* browser) {
  DCHECK(browsers_with_open_prompts_.count(browser));
  browsers_with_open_prompts_.erase(browser);
}

bool PrivacySandboxService::IsPromptOpenForBrowser(Browser* browser) {
  return browsers_with_open_prompts_.count(browser);
}

void PrivacySandboxService::SetPromptDisabledForTests(bool disabled) {
  g_prompt_disabled_for_tests = disabled;
}

void PrivacySandboxService::ForceChromeBuildForTests(bool force_chrome_build) {
  force_chrome_build_for_tests_ = force_chrome_build;
}

bool PrivacySandboxService::IsPrivacySandboxEnabled() {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabledV2);
}

bool PrivacySandboxService::IsPrivacySandboxManaged() {
  return pref_service_->IsManagedPreference(
      prefs::kPrivacySandboxApisEnabledV2);
}

bool PrivacySandboxService::IsPrivacySandboxRestricted() {
  return privacy_sandbox_settings_->IsPrivacySandboxRestricted();
}

void PrivacySandboxService::SetPrivacySandboxEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxManuallyControlledV2, true);
  privacy_sandbox_settings_->SetPrivacySandboxEnabled(enabled);
}

void PrivacySandboxService::OnPrivacySandboxV2PrefChanged() {
  // If the user has disabled the Privacy Sandbox, any data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabledV2)) {
    return;
  }

  if (browsing_data_remover_) {
    browsing_data_remover_->Remove(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }

  if (browsing_topics_service_) {
    browsing_topics_service_->ClearAllTopicsData();
  }
}

bool PrivacySandboxService::IsFirstPartySetsDataAccessEnabled() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled);
}

bool PrivacySandboxService::IsFirstPartySetsDataAccessManaged() const {
  return pref_service_->IsManagedPreference(
      prefs::kPrivacySandboxFirstPartySetsEnabled);
}

void PrivacySandboxService::SetFirstPartySetsDataAccessEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                            enabled);
}

void PrivacySandboxService::GetFledgeJoiningEtldPlusOneForDisplay(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  if (!interest_group_manager_) {
    std::move(callback).Run({});
    return;
  }

  interest_group_manager_->GetAllInterestGroupDataKeys(base::BindOnce(
      &PrivacySandboxService::ConvertInterestGroupDataKeysForDisplay,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<std::string>
PrivacySandboxService::GetBlockedFledgeJoiningTopFramesForDisplay() const {
  const base::Value::Dict& pref_value =
      pref_service_->GetDict(prefs::kPrivacySandboxFledgeJoinBlocked);

  std::vector<std::string> blocked_top_frames;

  for (auto entry : pref_value) {
    blocked_top_frames.emplace_back(entry.first);
  }

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

void PrivacySandboxService::RecordFirstPartySetsStateHistogram(
    PrivacySandboxService::FirstPartySetsState state) {
  base::UmaHistogramEnumeration("Settings.FirstPartySets.State", state);
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
        sandbox_v2_enabled ? PSStartupStates::kPromptOffV1OffEnabled
                           : PSStartupStates::kPromptOffV1OffDisabled);
    return;
  }
  // Handle 3PC disabled.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked)) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_startup_histogram,
        sandbox_v2_enabled ? PSStartupStates::kPromptOff3PCOffEnabled
                           : PSStartupStates::kPromptOff3PCOffDisabled);
    return;
  }
  // Handle managed.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxManaged)) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_startup_histogram,
        sandbox_v2_enabled ? PSStartupStates::kPromptOffManagedEnabled
                           : PSStartupStates::kPromptOffManagedDisabled);
    return;
  }
  // Handle restricted.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxRestricted)) {
    base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                  PSStartupStates::kPromptOffRestricted);
    return;
  }
  // Handle manually controlled
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationManuallyControlled)) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_startup_histogram,
        sandbox_v2_enabled
            ? PSStartupStates::kPromptOffManuallyControlledEnabled
            : PSStartupStates::kPromptOffManuallyControlledDisabled);
    return;
  }
  if (privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get()) {
    if (!pref_service_->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade)) {
      base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                    PSStartupStates::kPromptWaiting);
      return;
    }
    base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                  sandbox_v2_enabled
                                      ? PSStartupStates::kConsentShownEnabled
                                      : PSStartupStates::kConsentShownDisabled);
  } else if (privacy_sandbox::kPrivacySandboxSettings3NoticeRequired.Get()) {
    if (!pref_service_->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed)) {
      base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                    PSStartupStates::kPromptWaiting);
      return;
    }
    base::UmaHistogramEnumeration(privacy_sandbox_startup_histogram,
                                  sandbox_v2_enabled
                                      ? PSStartupStates::kNoticeShownEnabled
                                      : PSStartupStates::kNoticeShownDisabled);
  } else {  // No prompt currently required.
    base::UmaHistogramEnumeration(
        privacy_sandbox_startup_histogram,
        sandbox_v2_enabled ? PSStartupStates::kNoPromptRequiredEnabled
                           : PSStartupStates::kNoPromptRequiredDisabled);
  }
}

void PrivacySandboxService::RecordPrivacySandbox4StartupMetrics() {
  // Record the status of the APIs.
  const bool topics_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled);
  base::UmaHistogramBoolean("Settings.PrivacySandbox.Topics.Enabled",
                            topics_enabled);
  base::UmaHistogramBoolean(
      "Settings.PrivacySandbox.Fledge.Enabled",
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  base::UmaHistogramBoolean(
      "Settings.PrivacySandbox.AdMeasurement.Enabled",
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));

  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Prompt suppressed cases.
  PromptSuppressedReason prompt_suppressed_reason =
      static_cast<PromptSuppressedReason>(
          pref_service_->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed));

  switch (prompt_suppressed_reason) {
    // Prompt never suppressed.
    case PromptSuppressedReason::kNone: {
      break;
    }

    case PromptSuppressedReason::kRestricted: {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::kPromptNotShownDueToPrivacySandboxRestricted);
      return;
    }

    case PromptSuppressedReason::kThirdPartyCookiesBlocked: {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::kPromptNotShownDueTo3PCBlocked);
      return;
    }

    case PromptSuppressedReason::kTrialsConsentDeclined: {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::kPromptNotShownDueToTrialConsentDeclined);
      return;
    }

    case PromptSuppressedReason::kTrialsDisabledAfterNotice: {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::
              kPromptNotShownDueToTrialsDisabledAfterNoticeShown);
      return;
    }

    case PromptSuppressedReason::kPolicy: {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::kPromptNotShownDueToManagedState);
      return;
    }

    case PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration: {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          topics_enabled
              ? PromptStartupState::kEEAFlowCompletedWithTopicsAccepted
              : PromptStartupState::kEEAFlowCompletedWithTopicsDeclined);
      return;
    }

    case PromptSuppressedReason::
        kROWFlowCompletedAndTopicsDisabledBeforeEEAMigration: {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::kROWNoticeFlowCompleted);
      return;
    }
  }

  // Prompt was not suppressed explicitly at this point.
  CHECK_EQ(prompt_suppressed_reason, PromptSuppressedReason::kNone);

  // Check if prompt was suppressed implicitly.
  if (IsM1PrivacySandboxEffectivelyManaged(pref_service_)) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_prompt_startup_histogram,
        PromptStartupState::kPromptNotShownDueToManagedState);
    return;
  }

  // EEA
  if (privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get()) {
    // Consent decision not made
    if (!pref_service_->GetBoolean(
            prefs::kPrivacySandboxM1ConsentDecisionMade)) {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::kEEAConsentPromptWaiting);
      return;
    }

    // Consent decision made at this point.

    // Notice Acknowledged
    const bool notice_acknowledged = pref_service_->GetBoolean(
        prefs::kPrivacySandboxM1EEANoticeAcknowledged);
    if (notice_acknowledged) {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          topics_enabled
              ? PromptStartupState::kEEAFlowCompletedWithTopicsAccepted
              : PromptStartupState::kEEAFlowCompletedWithTopicsDeclined);
    } else {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PrivacySandboxService::PromptStartupState::kEEANoticePromptWaiting);
    }
    return;
  }

  // ROW
  if (privacy_sandbox::kPrivacySandboxSettings4NoticeRequired.Get()) {
    const bool row_notice_acknowledged = pref_service_->GetBoolean(
        prefs::kPrivacySandboxM1RowNoticeAcknowledged);

    base::UmaHistogramEnumeration(
        privacy_sandbox_prompt_startup_histogram,
        row_notice_acknowledged ? PromptStartupState::kROWNoticeFlowCompleted
                                : PromptStartupState::kROWNoticePromptWaiting);
    return;
  }
}

void PrivacySandboxService::LogPrivacySandboxState() {
  // Do not record metrics for non-regular profiles.
  if (!IsRegularProfile(profile_type_)) {
    return;
  }

  auto fps_status = FirstPartySetsState::kFpsNotRelevant;
  if (cookie_settings_->ShouldBlockThirdPartyCookies() &&
      cookie_settings_->GetDefaultCookieSetting(/*provider_id=*/nullptr) !=
          CONTENT_SETTING_BLOCK) {
    fps_status =
        pref_service_->GetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled)
            ? FirstPartySetsState::kFpsEnabled
            : FirstPartySetsState::kFpsDisabled;
  }
  RecordFirstPartySetsStateHistogram(fps_status);

  // Start by recording any metrics for Privacy Sandbox.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4)) {
    RecordPrivacySandbox4StartupMetrics();
    return;
  } else {
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
    if (default_cookie_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxService::SettingsPrivacySandboxEnabled::
              kPSEnabledBlockAll);
    } else if (cookie_controls_mode_value ==
               content_settings::CookieControlsMode::kBlockThirdParty) {
      RecordPrivacySandboxHistogram(
          PrivacySandboxService::SettingsPrivacySandboxEnabled::
              kPSEnabledBlock3P);
    } else {
      RecordPrivacySandboxHistogram(
          PrivacySandboxService::SettingsPrivacySandboxEnabled::
              kPSEnabledAllowAll);
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

void PrivacySandboxService::ConvertInterestGroupDataKeysForDisplay(
    base::OnceCallback<void(std::vector<std::string>)> callback,
    std::vector<content::InterestGroupManager::InterestGroupDataKey>
        data_keys) {
  std::set<std::string> display_entries;
  for (const auto& data_key : data_keys) {
    // When displaying interest group information in settings, the joining
    // origin is the relevant origin.
    const auto& origin = data_key.joining_origin;

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

  // Entries should be displayed alphabetically, as |display_entries| is a
  // std::set<std::string>, entries are already ordered correctly.
  std::move(callback).Run(
      std::vector<std::string>{display_entries.begin(), display_entries.end()});
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxService::GetCurrentTopTopics() const {
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get() ||
      (pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled) &&
       privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting
           .Get())) {
    return {fake_current_topics_.begin(), fake_current_topics_.end()};
  }

  if (!browsing_topics_service_) {
    return {};
  }

  auto topics = browsing_topics_service_->GetTopTopicsForDisplay();

  // Topics returned by the backend may include duplicates. Sort into display
  // order before removing them.
  SortTopicsForDisplay(topics);
  topics.erase(std::unique(topics.begin(), topics.end()), topics.end());

  return topics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxService::GetBlockedTopics() const {
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get() ||
      privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
    return {fake_blocked_topics_.begin(), fake_blocked_topics_.end()};
  }

  const base::Value::List& pref_value =
      pref_service_->GetList(prefs::kPrivacySandboxBlockedTopics);

  std::vector<privacy_sandbox::CanonicalTopic> blocked_topics;
  for (const auto& entry : pref_value) {
    auto blocked_topic = privacy_sandbox::CanonicalTopic::FromValue(
        *entry.GetDict().Find(kBlockedTopicsTopicKey));
    if (blocked_topic) {
      blocked_topics.emplace_back(*blocked_topic);
    }
  }

  SortTopicsForDisplay(blocked_topics);
  return blocked_topics;
}

void PrivacySandboxService::SetTopicAllowed(
    privacy_sandbox::CanonicalTopic topic,
    bool allowed) {
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get() ||
      privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
    if (allowed) {
      fake_current_topics_.insert(topic);
      fake_blocked_topics_.erase(topic);
    } else {
      fake_current_topics_.erase(topic);
      fake_blocked_topics_.insert(topic);
    }
    return;
  }

  if (!allowed && browsing_topics_service_) {
    browsing_topics_service_->ClearTopic(topic);
  }

  privacy_sandbox_settings_->SetTopicAllowed(topic, allowed);
}

base::flat_map<net::SchemefulSite, net::SchemefulSite>
PrivacySandboxService::GetSampleFirstPartySets() const {
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get() &&
      IsFirstPartySetsDataAccessEnabled()) {
    return {{net::SchemefulSite(GURL("https://youtube.com")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://google.com")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://google.com.au")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://google.de")),
             net::SchemefulSite(GURL("https://google.com"))},
            {net::SchemefulSite(GURL("https://chromium.org")),
             net::SchemefulSite(GURL("https://chromium.org"))},
            {net::SchemefulSite(GURL("https://googlesource.com")),
             net::SchemefulSite(GURL("https://chromium.org"))}};
  }

  return {};
}

absl::optional<net::SchemefulSite> PrivacySandboxService::GetFirstPartySetOwner(
    const GURL& site_url) const {
  // If FPS is not affecting cookie access, then there are effectively no
  // first party sets.
  if (!(cookie_settings_->ShouldBlockThirdPartyCookies() &&
        cookie_settings_->GetDefaultCookieSetting(/*provider_id=*/nullptr) !=
            CONTENT_SETTING_BLOCK &&
        base::FeatureList::IsEnabled(
            privacy_sandbox::kPrivacySandboxFirstPartySetsUI))) {
    return absl::nullopt;
  }

  // Return the owner according to the sample sets if they're provided.
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get()) {
    const base::flat_map<net::SchemefulSite, net::SchemefulSite> sets =
        GetSampleFirstPartySets();
    net::SchemefulSite schemeful_site(site_url);

    base::flat_map<net::SchemefulSite, net::SchemefulSite>::const_iterator
        site_entry = sets.find(schemeful_site);
    if (site_entry == sets.end()) {
      return absl::nullopt;
    }

    return site_entry->second;
  }

  absl::optional<net::FirstPartySetEntry> site_entry =
      first_party_sets_policy_service_->FindEntry(net::SchemefulSite(site_url));
  if (!site_entry.has_value()) {
    return absl::nullopt;
  }

  return site_entry->primary();
}

absl::optional<std::u16string>
PrivacySandboxService::GetFirstPartySetOwnerForDisplay(
    const GURL& site_url) const {
  absl::optional<net::SchemefulSite> site_owner =
      GetFirstPartySetOwner(site_url);
  if (!site_owner.has_value()) {
    return absl::nullopt;
  }

  // TODO(crbug.com/1332513): Apply formatting that correctly displays unicode
  // domains.
  return base::UTF8ToUTF16(site_owner->GetURL().host());
}

bool PrivacySandboxService::IsPartOfManagedFirstPartySet(
    const net::SchemefulSite& site) const {
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get()) {
    return IsFirstPartySetsDataAccessManaged() ||
           GetSampleFirstPartySets()[site] ==
               net::SchemefulSite(GURL("https://chromium.org"));
  }

  return first_party_sets_policy_service_->IsSiteInManagedSet(site);
}

void PrivacySandboxService::TopicsToggleChanged(bool new_value) const {
  RecordUpdatedTopicsConsent(
      privacy_sandbox::TopicsConsentUpdateSource::kSettings, new_value);
}

bool PrivacySandboxService::TopicsConsentRequired() const {
  return privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get();
}

bool PrivacySandboxService::TopicsHasActiveConsent() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxTopicsConsentGiven);
}

privacy_sandbox::TopicsConsentUpdateSource
PrivacySandboxService::TopicsConsentLastUpdateSource() const {
  return static_cast<privacy_sandbox::TopicsConsentUpdateSource>(
      pref_service_->GetInteger(
          prefs::kPrivacySandboxTopicsConsentLastUpdateReason));
}

base::Time PrivacySandboxService::TopicsConsentLastUpdateTime() const {
  return pref_service_->GetTime(
      prefs::kPrivacySandboxTopicsConsentLastUpdateTime);
}

std::string PrivacySandboxService::TopicsConsentLastUpdateText() const {
  return pref_service_->GetString(
      prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate);
}

/*static*/ PrivacySandboxService::PromptType
PrivacySandboxService::GetRequiredPromptTypeInternal(
    PrefService* pref_service,
    profile_metrics::BrowserProfileType profile_type,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    bool third_party_cookies_blocked) {
  DCHECK(
      !base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4));

  // If the prompt is disabled for testing, never show it.
  if (g_prompt_disabled_for_tests) {
    return PromptType::kNone;
  }

  // If the profile isn't a regular profile, no prompt should ever be shown.
  if (!IsRegularProfile(profile_type)) {
    return PromptType::kNone;
  }

  // Forced testing feature parameters override everything.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kDisablePrivacySandboxPrompts)) {
    return PromptType::kNone;
  }

  if (privacy_sandbox::kPrivacySandboxSettings3DisablePromptForTesting.Get()) {
    return PromptType::kNone;
  }

  if (privacy_sandbox::kPrivacySandboxSettings3ForceShowConsentForTesting
          .Get()) {
    return PromptType::kConsent;
  }

  if (privacy_sandbox::kPrivacySandboxSettings3ForceShowNoticeForTesting
          .Get()) {
    return PromptType::kNotice;
  }

  // If neither consent or notice is required, no prompt is required.
  if (!privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get() &&
      !privacy_sandbox::kPrivacySandboxSettings3NoticeRequired.Get()) {
    return PromptType::kNone;
  }

  // Only one of the consent or notice should be required by Finch parameters.
  DCHECK(!privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get() ||
         !privacy_sandbox::kPrivacySandboxSettings3NoticeRequired.Get());

  // Start by checking for any previous decision about the prompt, such as
  // it already having been shown, or not having been shown for some reason.
  // These checks for previous decisions occur in advance of their corresponding
  // decisions later in this function, so that changes to profile state to not
  // appear to impact previous decisions.

  // If a user wasn't shown a confirmation because they previously turned the
  // Privacy Sandbox off, we do not attempt to re-show one.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxDisabled)) {
    return PromptType::kNone;
  }

  // If a consent decision has already been made, no prompt is required.
  if (pref_service->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade)) {
    return PromptType::kNone;
  }

  // If only a notice is required, and has been shown, no prompt is required.
  if (!privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get() &&
      pref_service->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed)) {
    return PromptType::kNone;
  }

  // If a user wasn't shown a confirmation because the sandbox was previously
  // restricted, do not attempt to show them one. The user will be able to
  // enable the Sandbox on the settings page.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxRestricted)) {
    return PromptType::kNone;
  }

  // If a user wasn't shown a prompt previously because the Privacy Sandbox
  // was managed, do not show them one.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxManaged)) {
    return PromptType::kNone;
  }

  // If a user wasn't shown a confirmation because they block third party
  // cookies, we do not attempt to re-show one.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked)) {
    return PromptType::kNone;
  }

  // If the user wasn't shown a confirmation because they are already manually
  // controlling the sandbox, do not attempt to show one.
  if (pref_service->GetBoolean(
          prefs::kPrivacySandboxNoConfirmationManuallyControlled)) {
    return PromptType::kNone;
  }

  // If the Privacy Sandbox is restricted, no prompt is shown.
  if (privacy_sandbox_settings->IsPrivacySandboxRestricted()) {
    pref_service->SetBoolean(
        prefs::kPrivacySandboxNoConfirmationSandboxRestricted, true);
    return PromptType::kNone;
  }

  // If the Privacy Sandbox is managed, no prompt is shown.
  if (pref_service->FindPreference(prefs::kPrivacySandboxApisEnabledV2)
          ->IsManaged()) {
    pref_service->SetBoolean(prefs::kPrivacySandboxNoConfirmationSandboxManaged,
                             true);
    return PromptType::kNone;
  }

  // If the user blocks third party cookies, then no prompt is shown.
  if (third_party_cookies_blocked) {
    pref_service->SetBoolean(
        prefs::kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked, true);
    return PromptType::kNone;
  }

  // If the Privacy Sandbox has been manually controlled by the user, no prompt
  // is shown.
  if (pref_service->GetBoolean(prefs::kPrivacySandboxManuallyControlledV2)) {
    pref_service->SetBoolean(
        prefs::kPrivacySandboxNoConfirmationManuallyControlled, true);
    return PromptType::kNone;
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
    // restarts & updates via PromptActionOccurred() notwithstanding. To achieve
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
      return PromptType::kConsent;
    } else {
      DCHECK(!pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabledV2));
      pref_service->SetBoolean(
          prefs::kPrivacySandboxNoConfirmationSandboxDisabled, true);
      return PromptType::kNone;
    }
  }

  // At this point, no previous decision should have been made.
  DCHECK(!pref_service->GetBoolean(
      prefs::kPrivacySandboxNoConfirmationSandboxDisabled));
  DCHECK(!pref_service->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed));
  DCHECK(!pref_service->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade));

  // If the user had previously disabled the Privacy Sandbox, no confirmation
  // will be shown.
  if (!pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabled)) {
    pref_service->SetBoolean(
        prefs::kPrivacySandboxNoConfirmationSandboxDisabled, true);
    return PromptType::kNone;
  }

  // Check if the users requires a consent. This information is provided by
  // feature parameter to allow Finch based geo-targeting.
  if (privacy_sandbox::kPrivacySandboxSettings3ConsentRequired.Get()) {
    return PromptType::kConsent;
  }

  // Finally a notice is required.
  DCHECK(privacy_sandbox::kPrivacySandboxSettings3NoticeRequired.Get());
  return PromptType::kNotice;
}

/*static*/ PrivacySandboxService::PromptType
PrivacySandboxService::GetRequiredPromptTypeInternalM1(
    PrefService* pref_service,
    profile_metrics::BrowserProfileType profile_type,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    bool third_party_cookies_blocked,
    bool is_chrome_build) {
  DCHECK(
      base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4));

  // If the prompt is disabled for testing, never show it.
  if (g_prompt_disabled_for_tests) {
    return PromptType::kNone;
  }

  // If the profile isn't a regular profile, no prompt should ever be shown.
  if (!IsRegularProfile(profile_type)) {
    return PromptType::kNone;
  }

  // Forced testing feature parameters override everything.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kDisablePrivacySandboxPrompts)) {
    return PromptType::kNone;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowConsentForTesting
          .Get()) {
    return PromptType::kM1Consent;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowNoticeRowForTesting
          .Get()) {
    return PromptType::kM1NoticeROW;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowNoticeEeaForTesting
          .Get()) {
    return PromptType::kM1NoticeEEA;
  }

  // If this a non-Chrome build, do not show a prompt.
  if (!is_chrome_build) {
    return PromptType::kNone;
  }

  // If neither a notice nor a consent is required, do not show a prompt.
  if (!privacy_sandbox::kPrivacySandboxSettings4NoticeRequired.Get() &&
      !privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get()) {
    return PromptType::kNone;
  }

  // Only one of the consent or notice should be required by Finch parameters.
  DCHECK(!privacy_sandbox::kPrivacySandboxSettings4NoticeRequired.Get() ||
         !privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get());

  // If a prompt was suppressed once, for any reason, it will forever remain
  // suppressed and a prompt will not be shown.
  if (static_cast<PromptSuppressedReason>(
          pref_service->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed)) !=
      PromptSuppressedReason::kNone) {
    return PromptType::kNone;
  }

  // If an Admin controls any of the K-APIs or suppresses the prompt explicitly
  // then don't show the prompt.
  if (IsM1PrivacySandboxEffectivelyManaged(pref_service)) {
    return PromptType::kNone;
  }

  if (pref_service->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade) ||
      pref_service->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed)) {
    // If during the trials a previous consent decision was made, or the notice
    // was already acknowledged, and the privacy sandbox is disabled, set the
    // PromptSuppressedReason as appropriate and do not show a prompt.
    if (!pref_service->GetBoolean(prefs::kPrivacySandboxApisEnabledV2)) {
      int suppresed_reason =
          pref_service->GetBoolean(prefs::kPrivacySandboxNoticeDisplayed)
              ? static_cast<int>(
                    PromptSuppressedReason::kTrialsDisabledAfterNotice)
              : static_cast<int>(
                    PromptSuppressedReason::kTrialsConsentDeclined);
      pref_service->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                               suppresed_reason);
      return PromptType::kNone;
    }
  }

  // If third party cookies are blocked, set the suppression reason as such, and
  // do not show a prompt.
  if (third_party_cookies_blocked) {
    pref_service->SetInteger(
        prefs::kPrivacySandboxM1PromptSuppressed,
        static_cast<int>(PromptSuppressedReason::kThirdPartyCookiesBlocked));
    return PromptType::kNone;
  }

  // If the Privacy Sandbox is restricted, set the suppression reason as such,
  // and do not show a prompt.
  if (privacy_sandbox_settings->IsPrivacySandboxRestricted()) {
    pref_service->SetInteger(
        prefs::kPrivacySandboxM1PromptSuppressed,
        static_cast<int>(PromptSuppressedReason::kRestricted));
    return PromptType::kNone;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ConsentRequired.Get()) {
    if (pref_service->GetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade)) {
      // Since a consent decision has been made, if the eea notice has already
      // been acknowledged, do not show a prompt; else, show the eea notice.
      if (pref_service->GetBoolean(
              prefs::kPrivacySandboxM1EEANoticeAcknowledged)) {
        return PromptType::kNone;
      } else {
        return PromptType::kM1NoticeEEA;
      }
    } else {
      // A consent decision has not yet been made. If the user has seen a notice
      // and disabled Topics, we should not attempt to consent them. As they
      // already have sufficient notice for the other APIs, no prompt is
      // required.
      if (pref_service->GetBoolean(
              prefs::kPrivacySandboxM1RowNoticeAcknowledged) &&
          !pref_service->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)) {
        pref_service->SetInteger(
            prefs::kPrivacySandboxM1PromptSuppressed,
            static_cast<int>(
                PromptSuppressedReason::
                    kROWFlowCompletedAndTopicsDisabledBeforeEEAMigration));
        return PromptType::kNone;
      }
      return PromptType::kM1Consent;
    }
  }

  DCHECK(privacy_sandbox::kPrivacySandboxSettings4NoticeRequired.Get());

  // If a user that migrated from EEA to ROW has already completed the EEA
  // consent and notice flow, set the suppression reason as such and do not show
  // a prompt.
  if (pref_service->GetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade) &&
      (pref_service->GetBoolean(
          prefs::kPrivacySandboxM1EEANoticeAcknowledged))) {
    pref_service->SetInteger(
        prefs::kPrivacySandboxM1PromptSuppressed,
        static_cast<int>(
            PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration));
    return PromptType::kNone;
  }

  // If the notice has already been acknowledged, do not show a prompt.
  // Else, show the row notice prompt.
  if (pref_service->GetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged)) {
    return PromptType::kNone;
  } else {
    return PromptType::kM1NoticeROW;
  }
}

void PrivacySandboxService::MaybeInitializeFirstPartySetsPref() {
  // If initialization has already run, it is not required.
  if (pref_service_->GetBoolean(
          prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized)) {
    return;
  }

  // If the FPS UI is not available, no initialization is required.
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxFirstPartySetsUI)) {
    return;
  }

  // If the user blocks 3P cookies, disable the FPS data access preference.
  // As this logic relies on checking synced preference state, it is possible
  // that synced state is available when this decision is made. To err on the
  // side of privacy, this init logic is run per-device (the pref recording that
  // init has been run is not synced). If any of the user's devices local state
  // would disable the pref, it is disabled across all devices.
  if (AreThirdPartyCookiesBlocked(cookie_settings_)) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                              false);
  }

  pref_service_->SetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized, true);
}

void PrivacySandboxService::MaybeInitializeAntiAbuseContentSetting() {
  // If initialization has already run, it is not required.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxAntiAbuseInitialized)) {
    return;
  }

  // If the user blocks 3P cookies, disable the anti-abuse content setting.
  // As this logic relies on checking synced preference state, it is possible
  // that synced state is available when this decision is made. To err on the
  // side of privacy, this init logic is run per-device (the pref recording that
  // init has been run is not synced). If any of the user's devices local state
  // would disable the setting, it is disabled across all devices.
  if (AreThirdPartyCookiesBlocked(cookie_settings_)) {
    host_content_settings_map_->SetDefaultContentSetting(
        ContentSettingsType::ANTI_ABUSE, ContentSetting::CONTENT_SETTING_BLOCK);
  }

  pref_service_->SetBoolean(prefs::kPrivacySandboxAntiAbuseInitialized, true);
}

void PrivacySandboxService::RecordUpdatedTopicsConsent(
    privacy_sandbox::TopicsConsentUpdateSource source,
    bool did_consent) const {
  std::string consent_text;
  switch (source) {
    case (privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue): {
      NOTREACHED();
      break;
    }
    case (privacy_sandbox::TopicsConsentUpdateSource::kConfirmation): {
      consent_text = GetTopicsConfirmationText();
      break;
    }
    case (privacy_sandbox::TopicsConsentUpdateSource::kSettings): {
      int current_topics_count = GetCurrentTopTopics().size();
      int blocked_topics_count = GetBlockedTopics().size();
      consent_text = GetTopicsSettingsText(
          did_consent, current_topics_count > 0, blocked_topics_count > 0);
      break;
    }
    default:
      NOTREACHED();
  }

  pref_service_->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven,
                            did_consent);
  pref_service_->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                         base::Time::Now());
  pref_service_->SetInteger(prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
                            static_cast<int>(source));
  pref_service_->SetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate,
                           consent_text);
}

void PrivacySandboxService::InformSentimentService(
    PrivacySandboxService::PromptAction action) {
#if !BUILDFLAG(IS_ANDROID)
  if (!sentiment_service_) {
    return;
  }

  TrustSafetySentimentService::FeatureArea area;
  switch (action) {
    case PromptAction::kNoticeOpenSettings:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox3NoticeSettings;
      break;
    case PromptAction::kNoticeAcknowledge:
      area = TrustSafetySentimentService::FeatureArea::kPrivacySandbox3NoticeOk;
      break;
    case PromptAction::kNoticeDismiss:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox3NoticeDismiss;
      break;
    case PromptAction::kNoticeLearnMore:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox3NoticeLearnMore;
      break;
    case PromptAction::kConsentAccepted:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox3ConsentAccept;
      break;
    case PromptAction::kConsentDeclined:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox3ConsentDecline;
      break;
    default:
      return;
  }

  sentiment_service_->InteractedWithPrivacySandbox3(area);
#endif
}

void PrivacySandboxService::InformSentimentServiceM1(
    PrivacySandboxService::PromptAction action) {
#if !BUILDFLAG(IS_ANDROID)
  if (!sentiment_service_) {
    return;
  }

  TrustSafetySentimentService::FeatureArea area;
  switch (action) {
    case PromptAction::kNoticeOpenSettings:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox4NoticeSettings;
      break;
    case PromptAction::kNoticeAcknowledge:
      area = TrustSafetySentimentService::FeatureArea::kPrivacySandbox4NoticeOk;
      break;
    case PromptAction::kConsentAccepted:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox4ConsentAccept;
      break;
    case PromptAction::kConsentDeclined:
      area = TrustSafetySentimentService::FeatureArea::
          kPrivacySandbox4ConsentDecline;
      break;
    default:
      return;
  }

  sentiment_service_->InteractedWithPrivacySandbox4(area);
#endif
}

void PrivacySandboxService::RecordPromptActionMetrics(
    PrivacySandboxService::PromptAction action) {
  switch (action) {
    case (PromptAction::kNoticeShown): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.Shown"));
      break;
    }
    case (PromptAction::kNoticeOpenSettings): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.OpenedSettings"));
      break;
    }
    case (PromptAction::kNoticeAcknowledge): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.Acknowledged"));
      break;
    }
    case (PromptAction::kNoticeDismiss): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.Dismissed"));
      break;
    }
    case (PromptAction::kNoticeClosedNoInteraction): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.ClosedNoInteraction"));
      break;
    }
    case (PromptAction::kConsentShown): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Shown"));
      break;
    }
    case (PromptAction::kConsentAccepted): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Accepted"));
      break;
    }
    case (PromptAction::kConsentDeclined): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Declined"));
      break;
    }
    case (PromptAction::kConsentMoreInfoOpened): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.LearnMoreExpanded"));
      break;
    }
    case (PromptAction::kConsentMoreInfoClosed): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.LearnMoreClosed"));
      break;
    }
    case (PromptAction::kConsentClosedNoDecision): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.ClosedNoInteraction"));
      break;
    }
    case (PromptAction::kNoticeLearnMore): {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.LearnMore"));
      break;
    }
    case (PromptAction::kNoticeMoreInfoOpened): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.LearnMoreExpanded"));
      break;
    }
    case (PromptAction::kNoticeMoreInfoClosed): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.LearnMoreClosed"));
      break;
    }
    case (PromptAction::kConsentMoreButtonClicked): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.MoreButtonClicked"));
      break;
    }
    case (PromptAction::kNoticeMoreButtonClicked): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.MoreButtonClicked"));
      break;
    }
  }
}

void PrivacySandboxService::OnTopicsPrefChanged() {
  // If the user has disabled the preference, any related data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)) {
    return;
  }

  if (browsing_topics_service_) {
    browsing_topics_service_->ClearAllTopicsData();
  }
}

void PrivacySandboxService::OnFledgePrefChanged() {
  // If the user has disabled the preference, any related data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled)) {
    return;
  }

  if (browsing_data_remover_) {
    browsing_data_remover_->Remove(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS |
            content::BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE |
            content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS_INTERNAL,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }
}

void PrivacySandboxService::OnAdMeasurementPrefChanged() {
  // If the user has disabled the preference, any related data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled)) {
    return;
  }

  if (browsing_data_remover_) {
    browsing_data_remover_->Remove(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_ATTRIBUTION_REPORTING |
            content::BrowsingDataRemover::DATA_TYPE_AGGREGATION_SERVICE |
            content::BrowsingDataRemover::
                DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }
}

// static
bool PrivacySandboxService::IsM1PrivacySandboxEffectivelyManaged(
    PrefService* pref_service) {
  bool is_prompt_suppressed_by_policy =
      pref_service->IsManagedPreference(
          prefs::kPrivacySandboxM1PromptSuppressed) &&
      static_cast<int>(
          PrivacySandboxService::PromptSuppressedReason::kPolicy) ==
          pref_service->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed);

  return is_prompt_suppressed_by_policy ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1TopicsEnabled) ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1FledgeEnabled) ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1AdMeasurementEnabled);
}
