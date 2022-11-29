// Copyright 2021 The Chromium Authors
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
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/content_settings/core/browser/cookie_settings.h"
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
  if (profile_type != profile_metrics::BrowserProfileType::kRegular)
    return false;

#if BUILDFLAG(IS_CHROMEOS)
  // Any Device Local account, which is a CrOS concept powering things like
  // Kiosks and Managed Guest Sessions, is not considered regular.
  return !profiles::IsPublicSession() && !profiles::IsKioskSession() &&
         !profiles::IsChromeAppKioskSession();
#else
  return true;
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

  // If the Sandbox is currently restricted, disable the V2 preference. The user
  // must manually enable the sandbox if they stop being restricted.
  if (IsPrivacySandboxRestricted())
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);

  // Check for FPS pref init at each startup.
  // TODO(crbug.com/1351327): Remove this logic when most users have run init.
  MaybeInitializeFirstPartySetsPref();

  // Record preference state for UMA at each startup.
  LogPrivacySandboxState();
}

PrivacySandboxService::~PrivacySandboxService() = default;

PrivacySandboxService::PromptType
PrivacySandboxService::GetRequiredPromptType() {
  const auto third_party_cookies_blocked =
      AreThirdPartyCookiesBlocked(cookie_settings_);
  return GetRequiredPromptTypeInternal(pref_service_, profile_type_,
                                       privacy_sandbox_settings_,
                                       third_party_cookies_blocked);
}

void PrivacySandboxService::PromptActionOccurred(
    PrivacySandboxService::PromptAction action) {
  InformSentimentService(action);
  switch (action) {
    case (PromptAction::kNoticeShown): {
      // TODO(crbug.com/1378703): Handle new prompt types.
      if (PromptType::kNotice == GetRequiredPromptType()) {
        // The new Privacy Sandbox pref can be enabled when the notice has been
        // shown. Note that a notice will not have been shown if the user
        // disabled the old Privacy Sandbox pref.
        pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
        pref_service_->SetBoolean(prefs::kPrivacySandboxNoticeDisplayed, true);
      }
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
      pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxConsentDecisionMade,
                                true);
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Accepted"));
      break;
    }
    case (PromptAction::kConsentDeclined): {
      pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, false);
      pref_service_->SetBoolean(prefs::kPrivacySandboxConsentDecisionMade,
                                true);
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Declined"));
      break;
    }
    case (PromptAction::kConsentMoreInfoOpened): {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.LearnMoreExpanded"));
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
    // TODO(crbug.com/1378703): Clean up PromptAction and remove
    // *LearnMoreClosed or add use actions metrics for those prompt actions.
    default:
      break;
  }
}

// static
bool PrivacySandboxService::IsUrlSuitableForPrompt(const GURL& url) {
  // The prompt should be shown on a limited list of pages:

  // about:blank is valid.
  if (url.IsAboutBlank())
    return true;
  // Chrome settings page is valid. The subpages aren't as most of them are not
  // related to the prompt.
  if (url == GURL(chrome::kChromeUISettingsURL))
    return true;
  // Chrome history is valid as the prompt mentions history.
  if (url == GURL(chrome::kChromeUIHistoryURL))
    return true;
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
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabledV2))
    return;

  if (browsing_data_remover_) {
    browsing_data_remover_->Remove(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }

  if (browsing_topics_service_)
    browsing_topics_service_->ClearAllTopicsData();
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

  interest_group_manager_->GetAllInterestGroupJoiningOrigins(base::BindOnce(
      &PrivacySandboxService::ConvertFledgeJoiningTopFramesForDisplay,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<std::string>
PrivacySandboxService::GetBlockedFledgeJoiningTopFramesForDisplay() const {
  const base::Value::Dict& pref_value =
      pref_service_->GetDict(prefs::kPrivacySandboxFledgeJoinBlocked);

  std::vector<std::string> blocked_top_frames;

  for (auto entry : pref_value)
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

void PrivacySandboxService::LogPrivacySandboxState() {
  // Do not record metrics for non-regular profiles.
  if (!IsRegularProfile(profile_type_))
    return;

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

  // Start by recording any metrics for Privacy Sandbox 3.
  RecordPrivacySandbox3StartupMetrics();

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

  // Entries should be displayed alphabetically, as |display_entries| is a
  // std::set<std::string>, entries are already ordered correctly.
  std::move(callback).Run(
      std::vector<std::string>{display_entries.begin(), display_entries.end()});
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxService::GetCurrentTopTopics() const {
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get())
    return {fake_current_topics_.begin(), fake_current_topics_.end()};

  if (!browsing_topics_service_)
    return {};

  auto topics = browsing_topics_service_->GetTopTopicsForDisplay();

  // Topics returned by the backend may include duplicates. Sort into display
  // order before removing them.
  SortTopicsForDisplay(topics);
  topics.erase(std::unique(topics.begin(), topics.end()), topics.end());

  return topics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxService::GetBlockedTopics() const {
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get())
    return {fake_blocked_topics_.begin(), fake_blocked_topics_.end()};

  const base::Value::List& pref_value =
      pref_service_->GetList(prefs::kPrivacySandboxBlockedTopics);

  std::vector<privacy_sandbox::CanonicalTopic> blocked_topics;
  for (const auto& entry : pref_value) {
    auto blocked_topic = privacy_sandbox::CanonicalTopic::FromValue(
        *entry.GetDict().Find(kBlockedTopicsTopicKey));
    if (blocked_topic)
      blocked_topics.emplace_back(*blocked_topic);
  }

  SortTopicsForDisplay(blocked_topics);
  return blocked_topics;
}

void PrivacySandboxService::SetTopicAllowed(
    privacy_sandbox::CanonicalTopic topic,
    bool allowed) {
  if (privacy_sandbox::kPrivacySandboxSettings3ShowSampleDataForTesting.Get()) {
    if (allowed) {
      fake_current_topics_.insert(topic);
      fake_blocked_topics_.erase(topic);
    } else {
      fake_current_topics_.erase(topic);
      fake_blocked_topics_.insert(topic);
    }
    return;
  }

  if (!allowed && browsing_topics_service_)
    browsing_topics_service_->ClearTopic(topic);

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
    if (site_entry == sets.end())
      return absl::nullopt;

    return site_entry->second;
  }

  absl::optional<net::FirstPartySetEntry> site_entry =
      first_party_sets_policy_service_->FindEntry(net::SchemefulSite(site_url));
  if (!site_entry.has_value())
    return absl::nullopt;

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

/*static*/ PrivacySandboxService::PromptType
PrivacySandboxService::GetRequiredPromptTypeInternal(
    PrefService* pref_service,
    profile_metrics::BrowserProfileType profile_type,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    bool third_party_cookies_blocked) {
  // If the prompt is disabled for testing, never show it.
  if (g_prompt_disabled_for_tests)
    return PromptType::kNone;

  // If the profile isn't a regular profile, no prompt should ever be shown.
  if (!IsRegularProfile(profile_type))
    return PromptType::kNone;

  // Forced testing feature parameters override everything.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kDisablePrivacySandboxPrompts)) {
    return PromptType::kNone;
  }

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowConsentForTesting.Get())
    return PromptType::kM1Consent;

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowNoticeRowForTesting
          .Get())
    return PromptType::kM1NoticeROW;

  if (privacy_sandbox::kPrivacySandboxSettings4ForceShowNoticeEeaForTesting
          .Get())
    return PromptType::kM1NoticeEEA;

  if (privacy_sandbox::kPrivacySandboxSettings3DisablePromptForTesting.Get())
    return PromptType::kNone;

  if (privacy_sandbox::kPrivacySandboxSettings3ForceShowConsentForTesting.Get())
    return PromptType::kConsent;

  if (privacy_sandbox::kPrivacySandboxSettings3ForceShowNoticeForTesting.Get())
    return PromptType::kNotice;

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
  if (pref_service->GetBoolean(prefs::kPrivacySandboxConsentDecisionMade))
    return PromptType::kNone;

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

void PrivacySandboxService::InformSentimentService(
    PrivacySandboxService::PromptAction action) {
#if !BUILDFLAG(IS_ANDROID)
  if (!sentiment_service_)
    return;

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
