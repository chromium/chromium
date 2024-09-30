// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <algorithm>
#include <iterator>
#include <numeric>

#include "base/command_line.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_confirmation.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/common/semantic_tree.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
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
#include "privacy_sandbox_countries_impl.h"
#include "privacy_sandbox_service_impl.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "ui/views/widget/widget.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "base/json/values_util.h"
#include "base/time/time.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

namespace {

using PromptAction = ::PrivacySandboxService::PromptAction;
using SurfaceType = ::PrivacySandboxService::SurfaceType;

constexpr char kBlockedTopicsTopicKey[] = "topic";

bool g_prompt_disabled_for_tests = false;

bool IsFirstRunSuppressed(const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kNoFirstRun);
}

// Returns whether 3P cookies are blocked by |cookie_settings|. This can be
// either through blocking 3P cookies directly, or blocking all cookies.
// Blocking in this case also covers the "3P cookies limited" state.
bool ShouldBlockThirdPartyOrFirstPartyCookies(
    content_settings::CookieSettings* cookie_settings) {
  const auto default_content_setting =
      cookie_settings->GetDefaultCookieSetting();
  return cookie_settings->ShouldBlockThirdPartyCookies() ||
         default_content_setting == ContentSetting::CONTENT_SETTING_BLOCK;
}

// Similar to the function above, but checks for ALL 3P cookies to be blocked
// pre and post 3PCD.
bool AreAllThirdPartyCookiesBlocked(
    content_settings::CookieSettings* cookie_settings,
    PrefService* prefs,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings) {
  // Check if 1PCs are blocked.
  if (cookie_settings->GetDefaultCookieSetting() ==
      ContentSetting::CONTENT_SETTING_BLOCK) {
    return true;
  }
  // Check if all 3PCs are blocked.
  return tracking_protection_settings->AreAllThirdPartyCookiesBlocked() ||
         (!tracking_protection_settings->IsTrackingProtection3pcdEnabled() &&
          prefs->GetInteger(prefs::kCookieControlsMode) ==
              static_cast<int>(
                  content_settings::CookieControlsMode::kBlockThirdParty));
}

// Sorts |topics| alphabetically by topic display name for display.
// In addition, removes duplicate topics.
void SortAndDeduplicateTopicsForDisplay(
    std::vector<privacy_sandbox::CanonicalTopic>& topics) {
  std::sort(topics.begin(), topics.end(),
            [](const privacy_sandbox::CanonicalTopic& a,
               const privacy_sandbox::CanonicalTopic& b) {
              return a.GetLocalizedRepresentation() <
                     b.GetLocalizedRepresentation();
            });
  topics.erase(std::unique(topics.begin(), topics.end()), topics.end());
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
  return !chromeos::IsManagedGuestSession() && !chromeos::IsKioskSession() &&
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
      IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
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

void RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(bool value) {
  base::UmaHistogramBoolean(
      "PrivacySandbox.ProtectedAudience.JoiningTopFrameDisplayed", value);
}

constexpr std::string_view GetTopicsConsentNoticeName(
    SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kTopicsConsentModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::kTopicsConsentModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kTopicsConsentModalClankCCT;
    }
  }
}

constexpr std::string_view GetProtectedAudienceMeasurementNoticeName(
    SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kProtectedAudienceMeasurementNoticeModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::
          kProtectedAudienceMeasurementNoticeModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kProtectedAudienceMeasurementNoticeModalClankCCT;
    }
  }
}

constexpr std::string_view GetThreeAdsAPIsNoticeName(SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kThreeAdsAPIsNoticeModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::kThreeAdsAPIsNoticeModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kThreeAdsAPIsNoticeModalClankCCT;
    }
  }
}

constexpr std::string_view GetMeasurementNoticeName(SurfaceType surface_type) {
  switch (surface_type) {
    case SurfaceType::kDesktop: {
      return privacy_sandbox::kMeasurementNoticeModal;
    }
    case SurfaceType::kBrApp: {
      return privacy_sandbox::kMeasurementNoticeModalClankBrApp;
    }
    case SurfaceType::kAGACCT: {
      return privacy_sandbox::kMeasurementNoticeModalClankCCT;
    }
  }
}

std::string_view GetNoticeName(PromptAction action, SurfaceType surface_type) {
  std::string_view empty_view;
  switch (action) {
    case PromptAction::kConsentShown:
    case PromptAction::kConsentAccepted:
    case PromptAction::kConsentDeclined:
      return GetTopicsConsentNoticeName(surface_type);
    case PromptAction::kRestrictedNoticeShown:
    case PromptAction::kRestrictedNoticeAcknowledge:
    case PromptAction::kRestrictedNoticeOpenSettings:
      return GetMeasurementNoticeName(surface_type);
    case PromptAction::kNoticeShown:
    case PromptAction::kNoticeAcknowledge:
    case PromptAction::kNoticeOpenSettings:
      return privacy_sandbox::IsConsentRequired()
                 ? GetProtectedAudienceMeasurementNoticeName(surface_type)
                 : GetThreeAdsAPIsNoticeName(surface_type);
    default:
      return empty_view;
  }
}
}  // namespace

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

// static
void PrivacySandboxService::SetPromptDisabledForTests(bool disabled) {
  g_prompt_disabled_for_tests = disabled;
}

PrivacySandboxServiceImpl::PrivacySandboxServiceImpl(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    PrefService* pref_service,
    content::InterestGroupManager* interest_group_manager,
    profile_metrics::BrowserProfileType profile_type,
    content::BrowsingDataRemover* browsing_data_remover,
    HostContentSettingsMap* host_content_settings_map,
#if !BUILDFLAG(IS_ANDROID)
    TrustSafetySentimentService* sentiment_service,
#endif
    browsing_topics::BrowsingTopicsService* browsing_topics_service,
    first_party_sets::FirstPartySetsPolicyService* first_party_sets_service,
    PrivacySandboxCountries* privacy_sandbox_countries)
    : privacy_sandbox_settings_(privacy_sandbox_settings),
      tracking_protection_settings_(tracking_protection_settings),
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
      first_party_sets_policy_service_(first_party_sets_service),
      privacy_sandbox_countries_(privacy_sandbox_countries) {
  // Create notice storage
  notice_storage_ =
      std::make_unique<privacy_sandbox::PrivacySandboxNoticeStorage>();

  DCHECK(privacy_sandbox_settings_);
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);
  CHECK(tracking_protection_settings_);
  CHECK(notice_storage_);

  // Register observers for the Privacy Sandbox preferences.
  user_prefs_registrar_.Init(pref_service_);
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1TopicsEnabled,
      base::BindRepeating(&PrivacySandboxServiceImpl::OnTopicsPrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1FledgeEnabled,
      base::BindRepeating(&PrivacySandboxServiceImpl::OnFledgePrefChanged,
                          base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kPrivacySandboxM1AdMeasurementEnabled,
      base::BindRepeating(
          &PrivacySandboxServiceImpl::OnAdMeasurementPrefChanged,
          base::Unretained(this)));

  // If the Sandbox is currently restricted, disable it and reset any consent
  // information. The user must manually enable the sandbox if they stop being
  // restricted.
  if (IsPrivacySandboxRestricted()) {
    // Disable M1 prefs. Measurement pref should not be reset when restricted
    // notice feature is enabled.
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
    if (!privacy_sandbox::IsRestrictedNoticeRequired()) {
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                false);
    }

    // Clear any recorded consent information.
    pref_service_->ClearPref(prefs::kPrivacySandboxTopicsConsentGiven);
    pref_service_->ClearPref(prefs::kPrivacySandboxTopicsConsentLastUpdateTime);
    pref_service_->ClearPref(
        prefs::kPrivacySandboxTopicsConsentLastUpdateReason);
    pref_service_->ClearPref(
        prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate);
  }

  // kRestricted prompt suppression reason must be cleared at startup when
  // restricted notice feature is enabled.
  if (privacy_sandbox::IsRestrictedNoticeRequired() &&
      static_cast<PromptSuppressedReason>(
          pref_service->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed)) ==
          PromptSuppressedReason::kRestricted) {
    pref_service_->ClearPref(prefs::kPrivacySandboxM1PromptSuppressed);
  }

  // Check for FPS pref init at each startup.
  // TODO(crbug.com/40234448): Remove this logic when most users have run init.
  MaybeInitializeFirstPartySetsPref();

  // Record preference state for UMA at each startup.
  LogPrivacySandboxState();
}

PrivacySandboxServiceImpl::~PrivacySandboxServiceImpl() = default;

PrivacySandboxService::PromptType
// TODO(crbug.com/352575567): Use the SurfaceType passed in.
PrivacySandboxServiceImpl::GetRequiredPromptType(SurfaceType surface_type) {
  bool third_party_cookies_blocked = AreAllThirdPartyCookiesBlocked(
      cookie_settings_.get(), pref_service_, tracking_protection_settings_);
  return GetRequiredPromptTypeInternal(
      pref_service_, profile_type_, privacy_sandbox_settings_,
      third_party_cookies_blocked,
      force_chrome_build_for_tests_ || IsChromeBuild());
}

void UpdateNoticeStorage(
    PromptAction action,
    privacy_sandbox::PrivacySandboxNoticeStorage* notice_storage,
    PrefService* pref_service,
    SurfaceType surface_type) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPsDualWritePrefsToNoticeStorage)) {
    return;
  }

  // Set correct notice names, ready to receive and log PromptActions
  std::string_view notice_name = GetNoticeName(action, surface_type);

  switch (action) {
    // Topics notices (only shown for EEA, consent option)
    case PromptAction::kConsentShown: {
      notice_storage->SetNoticeShown(pref_service, notice_name,
                                     base::Time::Now());
      break;
    }
    case PromptAction::kConsentAccepted: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name, privacy_sandbox::NoticeActionTaken::kOptIn,
          base::Time::Now());
      break;
    }
    case PromptAction::kConsentDeclined: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kOptOut, base::Time::Now());
      break;
    }
    // EEA and ROW notices
    case PromptAction::kNoticeShown: {
      notice_storage->SetNoticeShown(pref_service, notice_name,
                                     base::Time::Now());
      break;
    }
    case PromptAction::kNoticeAcknowledge: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name, privacy_sandbox::NoticeActionTaken::kAck,
          base::Time::Now());
      break;
    }
    case PromptAction::kNoticeOpenSettings: {
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kSettings, base::Time::Now());
      break;
    }
    // Restricted notices
    case PromptAction::kRestrictedNoticeShown: {
      DCHECK(privacy_sandbox::IsRestrictedNoticeRequired());
      notice_storage->SetNoticeShown(pref_service, notice_name,
                                     base::Time::Now());
      break;
    }
    case PromptAction::kRestrictedNoticeAcknowledge: {
      DCHECK(privacy_sandbox::IsRestrictedNoticeRequired());
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name, privacy_sandbox::NoticeActionTaken::kAck,
          base::Time::Now());
      break;
    }
    case PromptAction::kRestrictedNoticeOpenSettings: {
      DCHECK(privacy_sandbox::IsRestrictedNoticeRequired());
      notice_storage->SetNoticeActionTaken(
          pref_service, notice_name,
          privacy_sandbox::NoticeActionTaken::kSettings, base::Time::Now());
      break;
    }
    default:
      break;
  }
}

void PrivacySandboxServiceImpl::PromptActionOccurred(PromptAction action,
                                                     SurfaceType surface_type) {
  RecordPromptActionMetrics(action);
  UpdateNoticeStorage(action, notice_storage_.get(), pref_service_.get(),
                      surface_type);

  InformSentimentService(action);
  if (PromptAction::kNoticeAcknowledge == action ||
      PromptAction::kNoticeOpenSettings == action) {
    if (privacy_sandbox::IsConsentRequired()) {
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
      DCHECK(privacy_sandbox::IsNoticeRequired());
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged,
                                true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
      pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                true);
    }
#if !BUILDFLAG(IS_ANDROID)
    MaybeCloseOpenPrompts();
#endif  // !BUILDFLAG(IS_ANDROID)
    // Consent-related PromptActions refer to to Topics Notice Consent
  } else if (PromptAction::kConsentAccepted == action) {
    DCHECK(privacy_sandbox::IsConsentRequired());
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade,
                              true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
    RecordUpdatedTopicsConsent(
        privacy_sandbox::TopicsConsentUpdateSource::kConfirmation, true);
  } else if (PromptAction::kConsentDeclined == action) {
    DCHECK(privacy_sandbox::IsConsentRequired());
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade,
                              true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    RecordUpdatedTopicsConsent(
        privacy_sandbox::TopicsConsentUpdateSource::kConfirmation, false);
  } else if (PromptAction::kRestrictedNoticeAcknowledge == action ||
             PromptAction::kRestrictedNoticeOpenSettings == action) {
    CHECK(privacy_sandbox::IsRestrictedNoticeRequired());
    pref_service_->SetBoolean(
        prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged, true);
    pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                              true);
#if !BUILDFLAG(IS_ANDROID)
    MaybeCloseOpenPrompts();
#endif  // !BUILDFLAG(IS_ANDROID)
  }
}

#if !BUILDFLAG(IS_ANDROID)
void PrivacySandboxServiceImpl::PromptOpenedForBrowser(Browser* browser,
                                                       views::Widget* widget) {
  DCHECK(!browsers_to_open_prompts_.count(browser));
  browsers_to_open_prompts_[browser] = widget;
}

void PrivacySandboxServiceImpl::PromptClosedForBrowser(Browser* browser) {
  DCHECK(browsers_to_open_prompts_.count(browser));
  browsers_to_open_prompts_.erase(browser);
}

bool PrivacySandboxServiceImpl::IsPromptOpenForBrowser(Browser* browser) {
  return browsers_to_open_prompts_.count(browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void PrivacySandboxServiceImpl::ForceChromeBuildForTests(
    bool force_chrome_build) {
  force_chrome_build_for_tests_ = force_chrome_build;
}

bool PrivacySandboxServiceImpl::IsPrivacySandboxRestricted() {
  return privacy_sandbox_settings_->IsPrivacySandboxRestricted();
}

bool PrivacySandboxServiceImpl::IsRestrictedNoticeEnabled() {
  return privacy_sandbox_settings_->IsRestrictedNoticeEnabled();
}

void PrivacySandboxServiceImpl::SetFirstPartySetsDataAccessEnabled(
    bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                            enabled);
}

bool PrivacySandboxServiceImpl::IsFirstPartySetsDataAccessEnabled() const {
  return privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled();
}

bool PrivacySandboxServiceImpl::IsFirstPartySetsDataAccessManaged() const {
  return pref_service_->IsManagedPreference(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
}

base::flat_map<net::SchemefulSite, net::SchemefulSite>
PrivacySandboxServiceImpl::GetSampleFirstPartySets() const {
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
             net::SchemefulSite(GURL("https://chromium.org"))},
            {net::SchemefulSite(GURL("https://muenchen.de")),
             net::SchemefulSite(GURL("https://xn--mnchen-3ya.de"))}};
  }

  return {};
}

std::optional<net::SchemefulSite>
PrivacySandboxServiceImpl::GetFirstPartySetOwner(const GURL& site_url) const {
  // If FPS is not affecting cookie access, then there are effectively no
  // first party sets.
  if (!(cookie_settings_->ShouldBlockThirdPartyCookies() &&
        cookie_settings_->GetDefaultCookieSetting() != CONTENT_SETTING_BLOCK &&
        base::FeatureList::IsEnabled(
            privacy_sandbox::kPrivacySandboxFirstPartySetsUI))) {
    return std::nullopt;
  }

  // Return the owner according to the sample sets if they're provided.
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get()) {
    const base::flat_map<net::SchemefulSite, net::SchemefulSite> sets =
        GetSampleFirstPartySets();
    net::SchemefulSite schemeful_site(site_url);

    base::flat_map<net::SchemefulSite, net::SchemefulSite>::const_iterator
        site_entry = sets.find(schemeful_site);
    if (site_entry == sets.end()) {
      return std::nullopt;
    }

    return site_entry->second;
  }

  std::optional<net::FirstPartySetEntry> site_entry =
      first_party_sets_policy_service_->FindEntry(net::SchemefulSite(site_url));
  if (!site_entry.has_value()) {
    return std::nullopt;
  }

  return site_entry->primary();
}

std::optional<std::u16string>
PrivacySandboxServiceImpl::GetFirstPartySetOwnerForDisplay(
    const GURL& site_url) const {
  std::optional<net::SchemefulSite> site_owner =
      GetFirstPartySetOwner(site_url);
  if (!site_owner.has_value()) {
    return std::nullopt;
  }

  return url_formatter::IDNToUnicode(site_owner->GetURL().host());
}

bool PrivacySandboxServiceImpl::IsPartOfManagedFirstPartySet(
    const net::SchemefulSite& site) const {
  if (privacy_sandbox::kPrivacySandboxFirstPartySetsUISampleSets.Get()) {
    return IsFirstPartySetsDataAccessManaged() ||
           GetSampleFirstPartySets()[site] ==
               net::SchemefulSite(GURL("https://chromium.org"));
  }

  return first_party_sets_policy_service_->IsSiteInManagedSet(site);
}

void PrivacySandboxServiceImpl::GetFledgeJoiningEtldPlusOneForDisplay(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  if (!interest_group_manager_) {
    std::move(callback).Run({});
    return;
  }

  interest_group_manager_->GetAllInterestGroupDataKeys(base::BindOnce(
      &PrivacySandboxServiceImpl::ConvertInterestGroupDataKeysForDisplay,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<std::string>
PrivacySandboxServiceImpl::GetBlockedFledgeJoiningTopFramesForDisplay() const {
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

void PrivacySandboxServiceImpl::SetFledgeJoiningAllowed(
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

void PrivacySandboxServiceImpl::RecordFirstPartySetsStateHistogram(
    FirstPartySetsState state) {
  base::UmaHistogramEnumeration("Settings.FirstPartySets.State", state);
}

void PrivacySandboxServiceImpl::RecordPrivacySandbox4StartupMetrics() {
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

  const bool user_reported_restricted =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1Restricted);
  const bool user_is_currently_unrestricted =
      privacy_sandbox_settings_->IsPrivacySandboxCurrentlyUnrestricted();

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

    case PromptSuppressedReason::kNoticeShownToGuardian: {
      // Check for users waiting for graduation: If a user was ever reported as
      // restricted and is currently unrestricted it means they are ready for
      // graduation.
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          user_reported_restricted && user_is_currently_unrestricted
              ? PromptStartupState::
                    kWaitingForGraduationRestrictedNoticeFlowNotCompleted
              : PromptStartupState::
                    kRestrictedNoticeNotShownDueToNoticeShownToGuardian);
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

  const bool restricted_notice_acknowledged = pref_service_->GetBoolean(
      prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged);

  // Check for users waiting for graduation: If a user was ever reported as
  // restricted and is currently unrestricted it means they are ready for
  // graduation.
  if (user_reported_restricted && user_is_currently_unrestricted) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_prompt_startup_histogram,
        restricted_notice_acknowledged
            ? PromptStartupState::
                  kWaitingForGraduationRestrictedNoticeFlowCompleted
            : PromptStartupState::
                  kWaitingForGraduationRestrictedNoticeFlowNotCompleted);

    return;
  }

  const bool row_notice_acknowledged =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged);
  const bool eaa_notice_acknowledged =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged);
  // Restricted Notice
  // Note that ordering is important: one of consent or notice will always be
  // required when the restricted prompt is shown, and both return
  // unconditionally.
  if (privacy_sandbox_settings_->IsSubjectToM1NoticeRestricted()) {
    // Acknowledgement of any of the prompt types implies acknowledgement of the
    // restricted notice as well.
    if (row_notice_acknowledged || eaa_notice_acknowledged) {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::
              kRestrictedNoticeNotShownDueToFullNoticeAcknowledged);

      return;
    }
    base::UmaHistogramEnumeration(
        privacy_sandbox_prompt_startup_histogram,
        restricted_notice_acknowledged
            ? PromptStartupState::kRestrictedNoticeFlowCompleted
            : PromptStartupState::kRestrictedNoticePromptWaiting);
    return;
  }

  // EEA
  if (privacy_sandbox::IsConsentRequired()) {
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
    if (eaa_notice_acknowledged) {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          topics_enabled
              ? PromptStartupState::kEEAFlowCompletedWithTopicsAccepted
              : PromptStartupState::kEEAFlowCompletedWithTopicsDeclined);
    } else {
      base::UmaHistogramEnumeration(
          privacy_sandbox_prompt_startup_histogram,
          PromptStartupState::kEEANoticePromptWaiting);
    }
    return;
  }

  // ROW
  if (privacy_sandbox::IsNoticeRequired()) {
    base::UmaHistogramEnumeration(
        privacy_sandbox_prompt_startup_histogram,
        row_notice_acknowledged ? PromptStartupState::kROWNoticeFlowCompleted
                                : PromptStartupState::kROWNoticePromptWaiting);
    return;
  }
}

void PrivacySandboxServiceImpl::LogPrivacySandboxState() {
  // Do not record metrics for non-regular profiles.
  if (!IsRegularProfile(profile_type_)) {
    return;
  }

  auto fps_status = FirstPartySetsState::kFpsNotRelevant;
  if (cookie_settings_->ShouldBlockThirdPartyCookies() &&
      cookie_settings_->GetDefaultCookieSetting() != CONTENT_SETTING_BLOCK) {
    fps_status = privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled()
                     ? FirstPartySetsState::kFpsEnabled
                     : FirstPartySetsState::kFpsDisabled;
  }
  RecordFirstPartySetsStateHistogram(fps_status);

  RecordPrivacySandbox4StartupMetrics();

  // TODO(crbug.com/333406690): After migration, move this portion to the
  // chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h constructor
  // and emit ALL startup histograms instead of just Topics consent related
  // histograms.
  for (const auto& notice_name : privacy_sandbox::kPrivacySandboxNoticeNames) {
    notice_storage_->RecordHistogramsOnStartup(pref_service_, notice_name);
  }
}

void PrivacySandboxServiceImpl::ConvertInterestGroupDataKeysForDisplay(
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
      RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(true);
      continue;
    }

    // The next best option is a host, which may be an IP address or an eTLD
    // itself (e.g. github.io).
    if (origin.host().length() > 0) {
      display_entries.emplace(origin.host());
      RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(true);
      continue;
    }

    // By design, each interest group should have a joining site or host, and
    // so this could ideally be a NOTREACHED(). However, following
    // crbug.com/1487191, it is apparent that this is not always true.
    // A host or site is expected in other parts of the UI, so we cannot simply
    // display the origin directly (it may also be empty). Instead, we elide it
    // but record a metric to understand how widespread this is.
    // TODO(crbug.com/40283983) - Investigate how much of an issue this is.
    RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(false);
  }

  // Entries should be displayed alphabetically, as |display_entries| is a
  // std::set<std::string>, entries are already ordered correctly.
  std::move(callback).Run(
      std::vector<std::string>{display_entries.begin(), display_entries.end()});
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetCurrentTopTopics() const {
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled) &&
      privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
    return {fake_current_topics_.begin(), fake_current_topics_.end()};
  }

  if (!browsing_topics_service_) {
    return {};
  }

  auto topics = browsing_topics_service_->GetTopTopicsForDisplay();
  SortAndDeduplicateTopicsForDisplay(topics);

  return topics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetBlockedTopics() const {
  if (privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
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

  SortAndDeduplicateTopicsForDisplay(blocked_topics);
  return blocked_topics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetFirstLevelTopics() const {
  static const base::NoDestructor<std::vector<privacy_sandbox::CanonicalTopic>>
      kFirstLevelTopics([]() -> std::vector<privacy_sandbox::CanonicalTopic> {
        browsing_topics::SemanticTree semantic_tree;

        auto topics = semantic_tree.GetFirstLevelTopicsInCurrentTaxonomy();
        std::vector<privacy_sandbox::CanonicalTopic> first_level_topics;
        first_level_topics.reserve(topics.size());
        std::transform(
            topics.begin(), topics.end(),
            std::back_inserter(first_level_topics),
            [&](const browsing_topics::Topic& topic) {
              return privacy_sandbox::CanonicalTopic(
                  topic, blink::features::kBrowsingTopicsTaxonomyVersion.Get());
            });

        SortAndDeduplicateTopicsForDisplay(first_level_topics);

        return first_level_topics;
      }());

  return *kFirstLevelTopics;
}

std::vector<privacy_sandbox::CanonicalTopic>
PrivacySandboxServiceImpl::GetChildTopicsCurrentlyAssigned(
    const privacy_sandbox::CanonicalTopic& parent_topic) const {
  browsing_topics::SemanticTree semantic_tree;

  auto descendant_topics =
      semantic_tree.GetDescendantTopics(parent_topic.topic_id());
  auto current_assigned_topics = GetCurrentTopTopics();

  std::set<privacy_sandbox::CanonicalTopic> descendant_topics_set;
  std::transform(
      std::begin(descendant_topics), std::end(descendant_topics),
      std::inserter(descendant_topics_set, descendant_topics_set.begin()),
      [](browsing_topics::Topic topic) {
        return privacy_sandbox::CanonicalTopic(
            topic, blink::features::kBrowsingTopicsTaxonomyVersion.Get());
      });
  std::vector<privacy_sandbox::CanonicalTopic> child_topics_assigned;
  for (const auto topic : current_assigned_topics) {
    if (descendant_topics_set.contains(topic)) {
      child_topics_assigned.push_back(topic);
    }
  }
  return child_topics_assigned;
}

void PrivacySandboxServiceImpl::SetTopicAllowed(
    privacy_sandbox::CanonicalTopic topic,
    bool allowed) {
  if (privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.Get()) {
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

PrivacySandboxCountries*
PrivacySandboxServiceImpl::GetPrivacySandboxCountries() {
  return privacy_sandbox_countries_;
}

bool PrivacySandboxServiceImpl::
    PrivacySandboxPrivacyGuideShouldShowAdTopicsCard() {
  return GetPrivacySandboxCountries()->IsConsentCountry() &&
         base::FeatureList::IsEnabled(
             privacy_sandbox::kPrivacySandboxPrivacyGuideAdTopics);
}

void PrivacySandboxServiceImpl::TopicsToggleChanged(bool new_value) const {
  RecordUpdatedTopicsConsent(
      privacy_sandbox::TopicsConsentUpdateSource::kSettings, new_value);
}

bool PrivacySandboxServiceImpl::TopicsConsentRequired() const {
  return privacy_sandbox::IsConsentRequired();
}

bool PrivacySandboxServiceImpl::TopicsHasActiveConsent() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxTopicsConsentGiven);
}

privacy_sandbox::TopicsConsentUpdateSource
PrivacySandboxServiceImpl::TopicsConsentLastUpdateSource() const {
  return static_cast<privacy_sandbox::TopicsConsentUpdateSource>(
      pref_service_->GetInteger(
          prefs::kPrivacySandboxTopicsConsentLastUpdateReason));
}

base::Time PrivacySandboxServiceImpl::TopicsConsentLastUpdateTime() const {
  return pref_service_->GetTime(
      prefs::kPrivacySandboxTopicsConsentLastUpdateTime);
}

std::string PrivacySandboxServiceImpl::TopicsConsentLastUpdateText() const {
  return pref_service_->GetString(
      prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate);
}

// static
PrivacySandboxService::PromptType
PrivacySandboxServiceImpl::GetRequiredPromptTypeInternal(
    PrefService* pref_service,
    profile_metrics::BrowserProfileType profile_type,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    bool third_party_cookies_blocked,
    bool is_chrome_build) {
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

  if (privacy_sandbox::
          kPrivacySandboxSettings4ForceShowNoticeRestrictedForTesting.Get()) {
    return PromptType::kM1NoticeRestricted;
  }

  // Suppress the prompt if we force --no-first-run for testing
  // and benchmarking.
  if (IsFirstRunSuppressed(*base::CommandLine::ForCurrentProcess())) {
    return PromptType::kNone;
  }

  // If this a non-Chrome build, do not show a prompt.
  if (!is_chrome_build) {
    return PromptType::kNone;
  }

  // If neither a notice nor a consent is required, do not show a prompt.
  if (!privacy_sandbox::IsNoticeRequired() &&
      !privacy_sandbox::IsConsentRequired()) {
    return PromptType::kNone;
  }

  // Only one of the consent or notice should be required.
  DCHECK(!privacy_sandbox::IsNoticeRequired() ||
         !privacy_sandbox::IsConsentRequired());

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
  if (privacy_sandbox_settings->IsPrivacySandboxRestricted() &&
      !privacy_sandbox::IsRestrictedNoticeRequired()) {
    pref_service->SetInteger(
        prefs::kPrivacySandboxM1PromptSuppressed,
        static_cast<int>(PromptSuppressedReason::kRestricted));
    return PromptType::kNone;
  }

  if (privacy_sandbox::IsRestrictedNoticeRequired()) {
    CHECK(privacy_sandbox::IsConsentRequired() ||
          privacy_sandbox::IsNoticeRequired());
    if (!pref_service->GetBoolean(
            prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged) &&
        !pref_service->GetBoolean(
            prefs::kPrivacySandboxM1EEANoticeAcknowledged) &&
        !pref_service->GetBoolean(
            prefs::kPrivacySandboxM1RowNoticeAcknowledged)) {
      if (privacy_sandbox_settings->IsSubjectToM1NoticeRestricted()) {
        return PromptType::kM1NoticeRestricted;
      }
      if (privacy_sandbox_settings->IsPrivacySandboxRestricted()) {
        pref_service->SetInteger(
            prefs::kPrivacySandboxM1PromptSuppressed,
            static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian));
        pref_service->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                 true);
        return PromptType::kNone;
      }
    }
  }

  if (privacy_sandbox::IsConsentRequired()) {
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

  DCHECK(privacy_sandbox::IsNoticeRequired());

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

  // If either the ROW notice or the restricted notice has already been
  // acknowledged, do not show a prompt. Else, show the row notice prompt.
  if (pref_service->GetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged) ||
      pref_service->GetBoolean(
          prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged)) {
    return PromptType::kNone;
  } else {
    return PromptType::kM1NoticeROW;
  }
}

void PrivacySandboxServiceImpl::MaybeInitializeFirstPartySetsPref() {
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
  if (ShouldBlockThirdPartyOrFirstPartyCookies(cookie_settings_.get())) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                              false);
  }

  pref_service_->SetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized, true);
}

void PrivacySandboxServiceImpl::RecordUpdatedTopicsConsent(
    privacy_sandbox::TopicsConsentUpdateSource source,
    bool did_consent) const {
  std::string consent_text;
  switch (source) {
    case privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
    case privacy_sandbox::TopicsConsentUpdateSource::kConfirmation: {
      consent_text = GetTopicsConfirmationText();
      break;
    }
    case privacy_sandbox::TopicsConsentUpdateSource::kSettings: {
      int current_topics_count = GetCurrentTopTopics().size();
      int blocked_topics_count = GetBlockedTopics().size();
      consent_text = GetTopicsSettingsText(
          did_consent, current_topics_count > 0, blocked_topics_count > 0);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
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

#if !BUILDFLAG(IS_ANDROID)
void PrivacySandboxServiceImpl::MaybeCloseOpenPrompts() {
  // Take a copy to avoid concurrent modification issues as widgets close and
  // remove themselves from the map synchronously. The map will typically have
  // at most a few elements, so this is cheap.
  // It is not possible that a new prompt may be added during this process, as
  // all prompts are created on the same thread, based on information which does
  // not cross task boundaries.
  auto browsers_to_open_prompts_copy = browsers_to_open_prompts_;
  for (const auto& browser_prompt : browsers_to_open_prompts_copy) {
    auto* prompt = browser_prompt.second.get();
    CHECK(prompt);
    prompt->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}
#endif

void PrivacySandboxServiceImpl::InformSentimentService(PromptAction action) {
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

void PrivacySandboxServiceImpl::RecordPromptActionMetrics(PromptAction action) {
  switch (action) {
    case PromptAction::kNoticeShown: {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.Shown"));
      break;
    }
    case PromptAction::kNoticeOpenSettings: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.OpenedSettings"));
      break;
    }
    case PromptAction::kNoticeAcknowledge: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.Acknowledged"));
      break;
    }
    case PromptAction::kNoticeDismiss: {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.Dismissed"));
      break;
    }
    case PromptAction::kNoticeClosedNoInteraction: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.ClosedNoInteraction"));
      break;
    }
    case PromptAction::kConsentShown: {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Shown"));
      break;
    }
    case PromptAction::kConsentAccepted: {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Accepted"));
      break;
    }
    case PromptAction::kConsentDeclined: {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Consent.Declined"));
      break;
    }
    case PromptAction::kConsentMoreInfoOpened: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.LearnMoreExpanded"));
      break;
    }
    case PromptAction::kConsentMoreInfoClosed: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.LearnMoreClosed"));
      break;
    }
    case PromptAction::kConsentClosedNoDecision: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.ClosedNoInteraction"));
      break;
    }
    case PromptAction::kNoticeLearnMore: {
      base::RecordAction(
          base::UserMetricsAction("Settings.PrivacySandbox.Notice.LearnMore"));
      break;
    }
    case PromptAction::kNoticeMoreInfoOpened: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.LearnMoreExpanded"));
      break;
    }
    case PromptAction::kNoticeMoreInfoClosed: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.LearnMoreClosed"));
      break;
    }
    case PromptAction::kConsentMoreButtonClicked: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.MoreButtonClicked"));
      break;
    }
    case PromptAction::kNoticeMoreButtonClicked: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Notice.MoreButtonClicked"));
      break;
    }
    case PromptAction::kRestrictedNoticeAcknowledge: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.RestrictedNotice.Acknowledged"));
      break;
    }
    case PromptAction::kRestrictedNoticeOpenSettings: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.RestrictedNotice.OpenedSettings"));
      break;
    }
    case PromptAction::kRestrictedNoticeShown: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.RestrictedNotice.Shown"));
      break;
    }
    case PromptAction::kRestrictedNoticeClosedNoInteraction: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.RestrictedNotice.ClosedNoInteraction"));
      break;
    }
    case PromptAction::kRestrictedNoticeMoreButtonClicked: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.RestrictedNotice.MoreButtonClicked"));
      break;
    }
    case PromptAction::kPrivacyPolicyLinkClicked: {
      base::RecordAction(base::UserMetricsAction(
          "Settings.PrivacySandbox.Consent.PrivacyPolicyLinkClicked"));
      break;
    }
  }
}

void PrivacySandboxServiceImpl::OnTopicsPrefChanged() {
  // If the user has disabled the preference, any related data stored should be
  // cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)) {
    return;
  }

  if (browsing_topics_service_) {
    browsing_topics_service_->ClearAllTopicsData();
  }
}

void PrivacySandboxServiceImpl::OnFledgePrefChanged() {
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

void PrivacySandboxServiceImpl::OnAdMeasurementPrefChanged() {
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
bool PrivacySandboxServiceImpl::IsM1PrivacySandboxEffectivelyManaged(
    PrefService* pref_service) {
  bool is_prompt_suppressed_by_policy =
      pref_service->IsManagedPreference(
          prefs::kPrivacySandboxM1PromptSuppressed) &&
      static_cast<int>(PromptSuppressedReason::kPolicy) ==
          pref_service->GetInteger(prefs::kPrivacySandboxM1PromptSuppressed);

  return is_prompt_suppressed_by_policy ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1TopicsEnabled) ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1FledgeEnabled) ||
         pref_service->IsManagedPreference(
             prefs::kPrivacySandboxM1AdMeasurementEnabled);
}

// TODO(b/341978070): Move Clank Activity Type Impl into it's own service.
#if BUILDFLAG(IS_ANDROID)
void RecordPercentageMetrics(const base::Value::List& activity_type_record) {
  using ActivityType = PrivacySandboxService::PrivacySandboxStorageActivityType;
  std::unordered_map<ActivityType, int> activity_type_counts{
      {ActivityType::kOther, 0},
      {ActivityType::kTabbed, 0},
      {ActivityType::kAGSACustomTab, 0},
      {ActivityType::kNonAGSACustomTab, 0},
      {ActivityType::kTrustedWebActivity, 0},
      {ActivityType::kWebapp, 0},
      {ActivityType::kWebApk, 0},
      {ActivityType::kPreFirstTab, 0}};

  for (const base::Value& record : activity_type_record) {
    std::optional<int> activity_type_int =
        record.GetDict().FindInt("activity_type");
    CHECK(activity_type_int.has_value());
    ActivityType activity_type =
        static_cast<ActivityType>(activity_type_int.value());
    activity_type_counts[activity_type]++;
  }

  std::unordered_map<ActivityType, int> activity_type_percentages;
  // Set each activity type percentage based on the count / total_records.
  for (const auto& [key, value] : activity_type_counts) {
    double raw_percentage = (value * 100.0) / activity_type_record.size();
    activity_type_percentages[key] = std::round(raw_percentage);
  }

  constexpr auto kTypesToHistogramSuffix =
      base::MakeFixedFlatMap<ActivityType, std::string_view>(
          {{ActivityType::kOther, "Other"},
           {ActivityType::kTabbed, "BrApp"},
           {ActivityType::kAGSACustomTab, "AGSACCT"},
           {ActivityType::kNonAGSACustomTab, "NonAGSACCT"},
           {ActivityType::kTrustedWebActivity, "TWA"},
           {ActivityType::kWebapp, "WebApp"},
           {ActivityType::kWebApk, "WebApk"},
           {ActivityType::kPreFirstTab, "PreFirstTab"}});

  // Emit all the histograms with each percentage value.
  for (const auto& [type, suffix] : kTypesToHistogramSuffix) {
    if (!activity_type_percentages.contains(type)) {
      return;
    }
    base::UmaHistogramPercentage(
        base::StrCat(
            {"PrivacySandbox.ActivityTypeStorage.Percentage.", suffix, "2"}),
        activity_type_percentages[type]);
  }
}

void RecordUserSegmentMetrics(const base::Value::List& activity_type_record,
                              int records_in_a_row) {
  // If a different value for records_in_a_row is needed for these metrics,
  // tools/metrics/histograms/metadata/privacy/histograms.xml needs to be
  // updated with new histograms. Currently, only
  // 10MostRecentRecordsUserSegment2 and 20MostRecentRecordsUserSegment2
  // histograms are necessary.
  DCHECK(records_in_a_row == 10 || records_in_a_row == 20);
  // Can't emit user segment metrics when the size of the list is less than
  // records_in_a_row
  if (activity_type_record.size() < static_cast<size_t>(records_in_a_row)) {
    return;
  }
  using ActivityType = PrivacySandboxService::PrivacySandboxStorageActivityType;
  using SegmentType =
      PrivacySandboxService::PrivacySandboxStorageUserSegmentByRecentActivity;

  // Helper function to get the activity type from a base::Value
  auto GetActivityType = [](const base::Value& record) -> ActivityType {
    std::optional<int> activity_type_int =
        record.GetDict().FindInt("activity_type");
    CHECK(activity_type_int.has_value());
    return static_cast<ActivityType>(activity_type_int.value());
  };

  std::unordered_set<ActivityType> encountered_activities;
  for (int i = 0; i < records_in_a_row; ++i) {
    encountered_activities.insert(GetActivityType(activity_type_record[i]));
  }

  SegmentType segment_type = SegmentType::kHasOther;
  if (encountered_activities.contains(ActivityType::kTabbed)) {
    segment_type = SegmentType::kHasBrowserApp;
  } else if (encountered_activities.contains(ActivityType::kAGSACustomTab)) {
    segment_type = SegmentType::kHasAGSACCT;
  } else if (encountered_activities.contains(ActivityType::kNonAGSACustomTab)) {
    segment_type = SegmentType::kHasNonAGSACCT;
  } else if (encountered_activities.contains(ActivityType::kWebApk)) {
    segment_type = SegmentType::kHasPWA;
  } else if (encountered_activities.contains(
                 ActivityType::kTrustedWebActivity)) {
    segment_type = SegmentType::kHasTWA;
  } else if (encountered_activities.contains(ActivityType::kWebapp)) {
    segment_type = SegmentType::kHasWebapp;
  } else if (encountered_activities.contains(ActivityType::kPreFirstTab)) {
    segment_type = SegmentType::kHasPreFirstTab;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.ActivityTypeStorage.",
                    base::NumberToString(records_in_a_row),
                    "MostRecentRecordsUserSegment2"}),
      segment_type);
}

void RecordDaysSinceMetrics(const base::Value::List& activity_type_record) {
  auto* timestamp =
      activity_type_record[activity_type_record.size() - 1].GetDict().Find(
          "timestamp");
  CHECK(timestamp);
  std::optional<base::Time> oldest_record_timestamp =
      base::ValueToTime(*timestamp);
  CHECK(oldest_record_timestamp.has_value());
  int days_since_oldest_record =
      (base::Time::Now() - oldest_record_timestamp.value()).InDays();
  base::UmaHistogramCustomCounts(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord",
      days_since_oldest_record, 1, 61, 60);
}

void RecordActivityTypeMetrics(const base::Value::List& activity_type_record,
                               base::Time current_time) {
  int total_records = static_cast<int>(activity_type_record.size());
  auto* oldest_record_timestamp_ptr =
      activity_type_record[total_records - 1].GetDict().Find("timestamp");
  CHECK(oldest_record_timestamp_ptr);
  std::optional<base::Time> oldest_record_timestamp =
      base::ValueToTime(*oldest_record_timestamp_ptr);
  base::Time uma_enabled_timestamp =
      base::Time::FromTimeT(g_browser_process->local_state()->GetInt64(
          metrics::prefs::kMetricsReportingEnabledTimestamp));
  // If a user has opted in, but the opt-in date is after the oldest record
  // timestamp in the activity type list, then no metrics should be emitted.
  if (oldest_record_timestamp.value() < uma_enabled_timestamp) {
    return;
  }
  // Min: 1, Max: 201 (exclusive), Buckets: 200 (in case the max total records
  // changes from 100).
  base::UmaHistogramCustomCounts(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength",
      static_cast<int>(activity_type_record.size()), 1, 201, 200);
  RecordPercentageMetrics(activity_type_record);
  RecordUserSegmentMetrics(activity_type_record, 10);
  RecordUserSegmentMetrics(activity_type_record, 20);
  RecordDaysSinceMetrics(activity_type_record);
}

void PrivacySandboxServiceImpl::RecordActivityType(
    PrivacySandboxStorageActivityType type) const {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived", type);

  // If skip-pre-first-tab is turned on, the list is not updated when the type
  // passed in is kPreFirstTab.
  if (type == PrivacySandboxService::PrivacySandboxStorageActivityType::
                  kPreFirstTab &&
      privacy_sandbox::kPrivacySandboxActivityTypeStorageSkipPreFirstTab
          .Get()) {
    return;
  }

  // Activity type launches can only be recorded if they fall within a specific
  // timeframe. This timeframe is determined by the within-x-days parameter,
  // where oldest_timestamp_allowed marks the end of the timeframe and
  // current_time marks the beginning.
  base::Time current_time = base::Time::Now();
  base::Time oldest_timestamp_allowed =
      current_time -
      base::Days(
          privacy_sandbox::kPrivacySandboxActivityTypeStorageWithinXDays.Get());

  base::Value::Dict new_dict;
  new_dict.Set("timestamp", base::TimeToValue(current_time));
  new_dict.Set("activity_type", static_cast<int>(type));

  const base::Value::List& old_activity_type_record =
      pref_service_->GetList(prefs::kPrivacySandboxActivityTypeRecord2);

  base::Value::List new_activity_type_record;
  new_activity_type_record.Append(std::move(new_dict));

  int last_n_launches =
      privacy_sandbox::kPrivacySandboxActivityTypeStorageLastNLaunches.Get();
  // The list is ordered from most recent records in the beginning of the list
  // and old records at the end of the list.
  for (const base::Value& child : old_activity_type_record) {
    const base::Value* child_timestamp_ptr = child.GetDict().Find("timestamp");
    if (!child_timestamp_ptr) {
      continue;
    }
    std::optional<base::Time> child_timestamp =
        base::ValueToTime(*child_timestamp_ptr);
    if (!child_timestamp.has_value()) {
      continue;
    }
    if (current_time >= child_timestamp.value() &&
        child_timestamp.value() >= oldest_timestamp_allowed &&
        new_activity_type_record.size() <
            static_cast<size_t>(last_n_launches)) {
      new_activity_type_record.Append(child.Clone());
    }
  }
  RecordActivityTypeMetrics(new_activity_type_record, current_time);
  pref_service_->SetList(prefs::kPrivacySandboxActivityTypeRecord2,
                         std::move(new_activity_type_record));
}
#endif  // BUILDFLAG(IS_ANDROID)
