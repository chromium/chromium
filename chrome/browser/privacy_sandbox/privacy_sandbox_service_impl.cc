// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_confirmation.h"
#include "chrome/browser/privacy_sandbox/profile_bucket_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/common/semantic_tree.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/interest_group_manager.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace {

using ::privacy_sandbox::EligibilityLevel;

using enum privacy_sandbox::EligibilityLevel;

constexpr char kBlockedTopicsTopicKey[] = "topic";

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
  // TODO(crbug.com/413388209): Update the LEARN_MORE_LINK to use a newer
  // version of the text and remove
  // `IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_LINK`
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
          ? IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_NEW
          : IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_V2;

  std::vector<int> string_ids = {
      IDS_SETTINGS_TOPICS_PAGE_TITLE,
      IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
      IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
      IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
      IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
      IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
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
        IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_V2);
  }

  return std::accumulate(string_ids.begin(), string_ids.end(), std::string(),
                         [](const std::string& previous_result, int next_id) {
                           auto next_string = l10n_util::GetStringUTF8(next_id);
                           return previous_result +
                                  (!previous_result.empty() ? " " : "") +
                                  next_string;
                         });
}

void RecordProtectedAudienceJoiningTopFrameDisplayedHistogram(bool value) {
  base::UmaHistogramBoolean(
      "PrivacySandbox.ProtectedAudience.JoiningTopFrameDisplayed", value);
}

// Emits startup histograms relating to the user's topics enabled status on
// both client and profile level.
void RecordTopicsEnabledHistograms(Profile* profile, bool enabled) {
  std::optional<privacy_sandbox::ProfileEnabledState> profile_enabled_state =
      privacy_sandbox::GetProfileEnabledState(profile, enabled);

  if (profile_enabled_state) {
    base::UmaHistogramEnumeration(
        "Settings.PrivacySandbox.Topics.EnabledForProfile",
        profile_enabled_state.value());
  }
  base::UmaHistogramBoolean("Settings.PrivacySandbox.Topics.Enabled", enabled);
}

// Emits startup histograms relating to the user's fledge enabled status on
// both client and profile level.
void RecordProtectedAudienceEnabledHistograms(Profile* profile, bool enabled) {
  std::optional<privacy_sandbox::ProfileEnabledState> profile_enabled_state =
      privacy_sandbox::GetProfileEnabledState(profile, enabled);

  if (profile_enabled_state) {
    base::UmaHistogramEnumeration(
        "Settings.PrivacySandbox.Fledge.EnabledForProfile",
        profile_enabled_state.value());
  }
  base::UmaHistogramBoolean("Settings.PrivacySandbox.Fledge.Enabled", enabled);
}

// Emits startup histograms relating to the user's AdMeasurement enabled
// status on both client and profile level.
void RecordAdMeasurementEnabledHistograms(Profile* profile, bool enabled) {
  std::optional<privacy_sandbox::ProfileEnabledState> profile_enabled_state =
      privacy_sandbox::GetProfileEnabledState(profile, enabled);

  if (profile_enabled_state) {
    base::UmaHistogramEnumeration(

        "Settings.PrivacySandbox.AdMeasurement.EnabledForProfile",
        profile_enabled_state.value());
  }
  base::UmaHistogramBoolean("Settings.PrivacySandbox.AdMeasurement.Enabled",
                            enabled);
}

}  // namespace

PrivacySandboxServiceImpl::PrivacySandboxServiceImpl(
    Profile* profile,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    PrefService* pref_service,
    content::InterestGroupManager* interest_group_manager,
    profile_metrics::BrowserProfileType profile_type,
    content::BrowsingDataRemover* browsing_data_remover,
    HostContentSettingsMap* host_content_settings_map,
    browsing_topics::BrowsingTopicsService* browsing_topics_service,
    first_party_sets::FirstPartySetsPolicyService* first_party_sets_service,
    PrivacySandboxCountries* privacy_sandbox_countries)
    : profile_(profile),
      privacy_sandbox_settings_(privacy_sandbox_settings),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      interest_group_manager_(interest_group_manager),
      profile_type_(profile_type),
      browsing_data_remover_(browsing_data_remover),
      host_content_settings_map_(host_content_settings_map),
      browsing_topics_service_(browsing_topics_service),
      first_party_sets_policy_service_(first_party_sets_service),
      privacy_sandbox_countries_(privacy_sandbox_countries) {
  static constexpr int kFakeTaxonomyVersion = 1;
  fake_current_topics_ = {{browsing_topics::Topic(1), kFakeTaxonomyVersion},
                          {browsing_topics::Topic(2), kFakeTaxonomyVersion}};
  fake_blocked_topics_ = {{browsing_topics::Topic(3), kFakeTaxonomyVersion},
                          {browsing_topics::Topic(4), kFakeTaxonomyVersion}};

  DCHECK(privacy_sandbox_settings_);
  DCHECK(pref_service_);
  DCHECK(cookie_settings_);
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
    if (!IsRestrictedNoticeRequired()) {
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

  // Check for FPS pref init at each startup.
  // TODO(crbug.com/40234448): Remove this logic when most users have run init.
  MaybeInitializeRelatedWebsiteSetsPref();

  // Record preference state for UMA at each startup.
  LogPrivacySandboxState();
}

PrivacySandboxServiceImpl::~PrivacySandboxServiceImpl() = default;

void PrivacySandboxServiceImpl::Shutdown() {
  user_prefs_registrar_.RemoveAll();
  privacy_sandbox_countries_ = nullptr;
  first_party_sets_policy_service_ = nullptr;
  browsing_topics_service_ = nullptr;
  host_content_settings_map_ = nullptr;
  browsing_data_remover_ = nullptr;
  interest_group_manager_ = nullptr;
  pref_service_ = nullptr;
  cookie_settings_ = nullptr;
  privacy_sandbox_settings_ = nullptr;
  profile_ = nullptr;
}

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

void PrivacySandboxServiceImpl::SetRelatedWebsiteSetsDataAccessEnabled(
    bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                            enabled);
}

bool PrivacySandboxServiceImpl::IsRelatedWebsiteSetsDataAccessEnabled() const {
  return privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled();
}

bool PrivacySandboxServiceImpl::IsRelatedWebsiteSetsDataAccessManaged() const {
  return pref_service_->IsManagedPreference(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
}

std::optional<net::SchemefulSite>
PrivacySandboxServiceImpl::GetRelatedWebsiteSetOwner(
    const GURL& site_url) const {
  // If RWS is not affecting cookie access, then there are effectively no
  // related website sets.
  if (!cookie_settings_->ShouldBlockThirdPartyCookies() ||
      cookie_settings_->GetDefaultCookieSetting() == CONTENT_SETTING_BLOCK) {
    return std::nullopt;
  }

  std::optional<net::FirstPartySetEntry> site_entry =
      first_party_sets_policy_service_->FindEntry(net::SchemefulSite(site_url));
  if (!site_entry.has_value()) {
    return std::nullopt;
  }

  return site_entry->primary();
}

std::optional<std::u16string>
PrivacySandboxServiceImpl::GetRelatedWebsiteSetOwnerForDisplay(
    const GURL& site_url) const {
  std::optional<net::SchemefulSite> site_owner =
      GetRelatedWebsiteSetOwner(site_url);
  if (!site_owner.has_value()) {
    return std::nullopt;
  }

  return url_formatter::IDNToUnicode(site_owner->GetURL().GetHost());
}

bool PrivacySandboxServiceImpl::IsPartOfManagedRelatedWebsiteSet(
    const net::SchemefulSite& site) const {
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
  const base::DictValue& pref_value =
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

void PrivacySandboxServiceImpl::RecordFirstPartySetsStateHistogram() {
  auto rws_status = FirstPartySetsState::kFpsNotRelevant;
  if (cookie_settings_->ShouldBlockThirdPartyCookies() &&
      cookie_settings_->GetDefaultCookieSetting() != CONTENT_SETTING_BLOCK) {
    rws_status = privacy_sandbox_settings_->AreRelatedWebsiteSetsEnabled()
                     ? FirstPartySetsState::kFpsEnabled
                     : FirstPartySetsState::kFpsDisabled;
  }
  base::UmaHistogramEnumeration("Settings.FirstPartySets.State", rws_status);
}

void PrivacySandboxServiceImpl::RecordTrackingProtectionStateHistogram() {
  base::UmaHistogramBoolean(
      "Settings.TrackingProtection.Enabled",
      pref_service_->GetBoolean(prefs::kTrackingProtection3pcdEnabled));
}

void PrivacySandboxServiceImpl::LogPrivacySandboxState() {
  // Do not record metrics for non-regular profiles.
  if (!IsRegularProfile(profile_type_)) {
    return;
  }
  RecordFirstPartySetsStateHistogram();
  RecordTrackingProtectionStateHistogram();

  // Record the status of the APIs.
  const bool topics_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled);
  const bool fledge_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled);
  const bool ad_measurement_enabled =
      pref_service_->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled);

  RecordTopicsEnabledHistograms(profile_, topics_enabled);
  RecordProtectedAudienceEnabledHistograms(profile_, fledge_enabled);
  RecordAdMeasurementEnabledHistograms(profile_, ad_measurement_enabled);
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
    // crbug.com/40933994, it is apparent that this is not always true.
    // A host or site is expected in other parts of the UI, so we cannot
    // simply display the origin directly (it may also be empty). Instead, we
    // elide it but record a metric to understand how widespread this is.
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

  const base::ListValue& pref_value =
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
  return GetPrivacySandboxCountries()->IsConsentCountry();
}

bool PrivacySandboxServiceImpl::ShouldUsePrivacyPolicyChinaDomain() {
  return GetPrivacySandboxCountries()->IsLatestCountryChina();
}

void PrivacySandboxServiceImpl::TopicsToggleChanged(bool new_value) const {
  RecordUpdatedTopicsConsent(
      privacy_sandbox::TopicsConsentUpdateSource::kSettings, new_value);
}

bool PrivacySandboxServiceImpl::TopicsConsentRequired() {
  return IsConsentRequired();
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

void PrivacySandboxServiceImpl::MaybeInitializeRelatedWebsiteSetsPref() {
  // If initialization has already run, it is not required.
  if (pref_service_->GetBoolean(
          prefs::
              kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized)) {
    return;
  }

  // If the user blocks 3P cookies, disable the RWS data access preference.
  // As this logic relies on checking synced preference state, it is possible
  // that synced state is available when this decision is made. To err on the
  // side of privacy, this init logic is run per-device (the pref recording
  // that init has been run is not synced). If any of the user's devices local
  // state would disable the pref, it is disabled across all devices.
  if (ShouldBlockThirdPartyOrFirstPartyCookies(cookie_settings_.get())) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                              false);
  }

  pref_service_->SetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized,
      true);
}

void PrivacySandboxServiceImpl::RecordUpdatedTopicsConsent(
    privacy_sandbox::TopicsConsentUpdateSource source,
    bool did_consent) const {
  std::string consent_text;
  switch (source) {
    case privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue: {
      NOTREACHED();
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

void PrivacySandboxServiceImpl::OnTopicsPrefChanged() {
  // If the user has disabled the preference, any related data stored should
  // be cleared.
  if (pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)) {
    return;
  }

  if (browsing_topics_service_) {
    browsing_topics_service_->ClearAllTopicsData();
  }
}

void PrivacySandboxServiceImpl::OnFledgePrefChanged() {
  // If the user has disabled the preference, any related data stored should
  // be cleared.
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
  // If the user has disabled the preference, any related data stored should
  // be cleared.
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

// We are intentionally not setting the old pref
// `kPrivacySandboxM1ConsentDecisionMade` here. This means that when switching
// to the new implementation, GetRequiredPromptType as part of the old PSService
// will no longer return the correct value due to its reliance on the old prefs.
// See go/notice-framework-migration-plan-onepager for more details on how we
// plan on doing a safe migration with this constraint.
void PrivacySandboxServiceImpl::UpdateTopicsApiResult(bool value) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, value);
}

// We are intentionally not setting the old prefs
// `PrivacySandboxM1EEANoticeAcknowledged` or
// `PrivacySandboxM1RowNoticeAcknowledged` here. This means that when switching
// to the new implementation, GetRequiredPromptType as part of the old
// PSService will no longer return the correct value due to its reliance on the
// old prefs. See go/notice-framework-migration-plan-onepager for more
// details on how we plan on doing a safe migration with this constraint
void PrivacySandboxServiceImpl::UpdateProtectedAudienceApiResult(bool value) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, value);
}

// We are intentionally not setting the old pref
// `PrivacySandboxM1EEANoticeAcknowledged`,
// `PrivacySandboxM1RowNoticeAcknowledged`,
// `PrivacySandboxM1RestrictedNoticeAcknowledged` here.
// This means that when switching to the new implementation,
// the GetRequiredPromptType as part of the old PSService will no longer return
// the correct value due to its reliance on the old prefs. See
// go/notice-framework-migration-plan-onepager for more details on the migration
// plan.
void PrivacySandboxServiceImpl::UpdateMeasurementApiResult(bool value) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                            value);
}

bool PrivacySandboxServiceImpl::IsConsentRequired() {
  return privacy_sandbox::IsConsentRequired(privacy_sandbox_countries_);
}

bool PrivacySandboxServiceImpl::IsNoticeRequired() {
  return privacy_sandbox::IsNoticeRequired(privacy_sandbox_countries_);
}

bool PrivacySandboxServiceImpl::IsRestrictedNoticeRequired() {
  return privacy_sandbox::IsRestrictedNoticeRequired(
      privacy_sandbox_countries_);
}

EligibilityLevel PrivacySandboxServiceImpl::GetTopicsApiEligibility() {
  return kNotEligible;
}

EligibilityLevel
PrivacySandboxServiceImpl::GetProtectedAudienceApiEligibility() {
  return kNotEligible;
}

EligibilityLevel PrivacySandboxServiceImpl::GetAdMeasurementApiEligibility() {
  return kNotEligible;
}
