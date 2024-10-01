// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_

// clang-format off
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries_impl.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
// clang-format on

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/interest_group_manager.h"
#include "net/base/schemeful_site.h"

class Browser;
class PrefService;
#if !BUILDFLAG(IS_ANDROID)
class TrustSafetySentimentService;
#endif

namespace content {
class BrowsingDataRemover;
}

namespace content_settings {
class CookieSettings;
}

namespace browsing_topics {
class BrowsingTopicsService;
}

namespace views {
class Widget;
}

class PrivacySandboxServiceImpl : public PrivacySandboxService {
 public:
  PrivacySandboxServiceImpl(
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
      PrivacySandboxCountries* privacy_sandbox_countries);

  ~PrivacySandboxServiceImpl() override;

  // PrivacySandboxService:
  PromptType GetRequiredPromptType(SurfaceType surface_type) override;
  void PromptActionOccurred(PromptAction action,
                            SurfaceType surface_type) override;
#if !BUILDFLAG(IS_ANDROID)
  void PromptOpenedForBrowser(Browser* browser, views::Widget* widget) override;
  void PromptClosedForBrowser(Browser* browser) override;
  bool IsPromptOpenForBrowser(Browser* browser) override;
#endif  // !BUILDFLAG(IS_ANDROID)
  void ForceChromeBuildForTests(bool force_chrome_build) override;
  bool IsPrivacySandboxRestricted() override;
  bool IsRestrictedNoticeEnabled() override;
  void SetFirstPartySetsDataAccessEnabled(bool enabled) override;
  bool IsFirstPartySetsDataAccessEnabled() const override;
  bool IsFirstPartySetsDataAccessManaged() const override;
  base::flat_map<net::SchemefulSite, net::SchemefulSite>
  GetSampleFirstPartySets() const override;
  std::optional<net::SchemefulSite> GetFirstPartySetOwner(
      const GURL& site_url) const override;
  std::optional<std::u16string> GetFirstPartySetOwnerForDisplay(
      const GURL& site_url) const override;
  bool IsPartOfManagedFirstPartySet(
      const net::SchemefulSite& site) const override;
  void GetFledgeJoiningEtldPlusOneForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback) override;
  std::vector<std::string> GetBlockedFledgeJoiningTopFramesForDisplay()
      const override;
  void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                               bool allowed) const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetCurrentTopTopics()
      const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetBlockedTopics()
      const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetFirstLevelTopics()
      const override;
  std::vector<privacy_sandbox::CanonicalTopic> GetChildTopicsCurrentlyAssigned(
      const privacy_sandbox::CanonicalTopic& topic) const override;
  void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                       bool allowed) override;
  bool PrivacySandboxPrivacyGuideShouldShowAdTopicsCard() override;
  void TopicsToggleChanged(bool new_value) const override;
  bool TopicsConsentRequired() const override;
  bool TopicsHasActiveConsent() const override;
  privacy_sandbox::TopicsConsentUpdateSource TopicsConsentLastUpdateSource()
      const override;
  base::Time TopicsConsentLastUpdateTime() const override;
  std::string TopicsConsentLastUpdateText() const override;
#if BUILDFLAG(IS_ANDROID)
  void RecordActivityType(
      PrivacySandboxStorageActivityType type) const override;
#endif  // BUILDFLAG(IS_ANDROID)

 protected:
  friend class PrivacySandboxServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           MetricsLoggingOccursCorrectly);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestNonRegularProfile,
                           NoMetricsRecorded);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest, RestrictedPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest, ManagedNoPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest,
                           ManuallyControlledNoPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServicePromptTest, NoParamNoPrompt);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDeathTest,
                           GetRequiredPromptType);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxPromptNoticeWaiting);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxPromptConsentWaiting);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxV1OffEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxV1OffDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxConsentEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxConsentDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoticeEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoticeDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandbox3PCOffEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandbox3PCOffDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManagedEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManagedDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManuallyControlledEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxManuallyControlledDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoPromptDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoPromptEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest, PrivacySandboxRestricted);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           FirstPartySetsNotRelevantMetricAllowedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           FirstPartySetsNotRelevantMetricBlockedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           FirstPartySetsEnabledMetric);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           FirstPartySetsDisabledMetric);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Explicitly);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Implicitly);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_EEA);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceTest,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_ROW);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RecordPrivacySandbox4StartupMetrics_APIs);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
                           ReturnsCorrectStatus);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticePromptTest,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
      RecordPrivacySandbox4StartupMetrics_GraduationFlow);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyRestricted,
      RecordPrivacySandbox4StartupMetrics_GraduationFlow);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
      RecordPrivacySandbox4StartupMetrics_GraduationFlowWhenNoticeShownToGuardian);

  // Contains all possible privacy sandbox states, recorded on startup.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Must be kept in sync with the SettingsPrivacySandboxEnabled enum in
  // histograms/enums.xml.
  enum class SettingsPrivacySandboxEnabled {
    kPSEnabledAllowAll = 0,
    kPSEnabledBlock3P = 1,
    kPSEnabledBlockAll = 2,
    kPSDisabledAllowAll = 3,
    kPSDisabledBlock3P = 4,
    kPSDisabledBlockAll = 5,
    kPSDisabledPolicyBlock3P = 6,
    kPSDisabledPolicyBlockAll = 7,
    // DEPRECATED
    kPSEnabledFlocDisabledAllowAll = 8,
    // DEPRECATED
    kPSEnabledFlocDisabledBlock3P = 9,
    // DEPRECATED
    kPSEnabledFlocDisabledBlockAll = 10,
    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kPSEnabledFlocDisabledBlockAll,
  };

  // Contains all possible states of first party sets preference.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Must be kept in sync with the FirstPartySetsState enum in
  // histograms/enums.xml.
  enum class FirstPartySetsState {
    // The user allows all cookies, or blocks all cookies.
    kFpsNotRelevant = 0,
    // The user blocks third-party cookies, and has FPS enabled.
    kFpsEnabled = 1,
    // The user blocks third-party cookies, and has FPS disabled.
    kFpsDisabled = 2,
    kMaxValue = kFpsDisabled,
  };

  // Contains the possible states of a users Privacy Sandbox overall settings.
  // Must be kept in sync with SettingsPrivacySandboxStartupStates in
  // histograms/enums.xml
  enum class PSStartupStates {
    kPromptWaiting = 0,
    kPromptOffV1OffEnabled = 1,
    kPromptOffV1OffDisabled = 2,
    kConsentShownEnabled = 3,
    kConsentShownDisabled = 4,
    kNoticeShownEnabled = 5,
    kNoticeShownDisabled = 6,
    kPromptOff3PCOffEnabled = 7,
    kPromptOff3PCOffDisabled = 8,
    kPromptOffManagedEnabled = 9,
    kPromptOffManagedDisabled = 10,
    kPromptOffRestricted = 11,
    kPromptOffManuallyControlledEnabled = 12,
    kPromptOffManuallyControlledDisabled = 13,
    kNoPromptRequiredEnabled = 14,
    kNoPromptRequiredDisabled = 15,

    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kNoPromptRequiredDisabled,
  };

  // Contains the possible states of the prompt start up states for m1.
  // Must be kept in sync with SettingsPrivacySandboxPromptStartupState in
  // histograms/enums.xml
  enum class PromptStartupState {
    kEEAConsentPromptWaiting = 0,
    kEEANoticePromptWaiting = 1,
    kROWNoticePromptWaiting = 2,
    kEEAFlowCompletedWithTopicsAccepted = 3,
    kEEAFlowCompletedWithTopicsDeclined = 4,
    kROWNoticeFlowCompleted = 5,
    kPromptNotShownDueToPrivacySandboxRestricted = 6,
    kPromptNotShownDueTo3PCBlocked = 7,
    kPromptNotShownDueToTrialConsentDeclined = 8,
    kPromptNotShownDueToTrialsDisabledAfterNoticeShown = 9,
    kPromptNotShownDueToManagedState = 10,
    kRestrictedNoticeNotShownDueToNoticeShownToGuardian = 11,
    kRestrictedNoticePromptWaiting = 12,
    kRestrictedNoticeFlowCompleted = 13,
    kRestrictedNoticeNotShownDueToFullNoticeAcknowledged = 14,
    kWaitingForGraduationRestrictedNoticeFlowNotCompleted = 15,
    kWaitingForGraduationRestrictedNoticeFlowCompleted = 16,

    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kWaitingForGraduationRestrictedNoticeFlowCompleted,
  };

  // Helper function to log first party sets state.
  void RecordFirstPartySetsStateHistogram(FirstPartySetsState state);

  // Logs the state of the privacy sandbox and cookie settings. Called once per
  // profile startup.
  void LogPrivacySandboxState();

  // Logs the state of privacy sandbox 4 in regards to prompts. Called once per
  // profile startup.
  void RecordPrivacySandbox4StartupMetrics();

  // Converts the provided list of |top_frames| into eTLD+1s for display, and
  // provides those to |callback|.
  void ConvertInterestGroupDataKeysForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<content::InterestGroupManager::InterestGroupDataKey>
          data_keys);

  // Contains the logic which powers GetRequiredPromptType(). Static to allow
  // EXPECT_DCHECK_DEATH testing, which does not work well with many of the
  // other dependencies of this service. It is also for this reason the 3P
  // cookie block state is passed in, as CookieSettings cannot be used in
  // death tests.
  static PrivacySandboxService::PromptType GetRequiredPromptTypeInternal(
      PrefService* pref_service,
      profile_metrics::BrowserProfileType profile_type,
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      bool third_party_cookies_blocked,
      bool is_chrome_build);

  // Checks to see if initialization of the user's FPS pref is required, and if
  // so, sets the default value based on the user's current cookie settings.
  void MaybeInitializeFirstPartySetsPref();

  // Updates the preferences which store the current Topics consent information.
  void RecordUpdatedTopicsConsent(
      privacy_sandbox::TopicsConsentUpdateSource source,
      bool did_consent) const;

#if !BUILDFLAG(IS_ANDROID)
  // If appropriate based on feature state, closes all currently open Privacy
  // Sandbox prompts.
  void MaybeCloseOpenPrompts();
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<content::InterestGroupManager> interest_group_manager_;
  profile_metrics::BrowserProfileType profile_type_;
  std::unique_ptr<privacy_sandbox::PrivacySandboxNoticeStorage> notice_storage_;
  raw_ptr<content::BrowsingDataRemover> browsing_data_remover_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<TrustSafetySentimentService> sentiment_service_;
#endif
  raw_ptr<browsing_topics::BrowsingTopicsService> browsing_topics_service_;
  raw_ptr<first_party_sets::FirstPartySetsPolicyService>
      first_party_sets_policy_service_;
  raw_ptr<PrivacySandboxCountries> privacy_sandbox_countries_;

  PrefChangeRegistrar user_prefs_registrar_;

#if !BUILDFLAG(IS_ANDROID)
  // A map of Browser windows which have an open Privacy Sandbox prompt,
  // to the Widget for that prompt.
  std::map<Browser*, raw_ptr<views::Widget, CtnExperimental>>
      browsers_to_open_prompts_;
#endif

  // Fake implementation for current and blocked topics.
  static constexpr int kFakeTaxonomyVersion = 1;
  std::set<privacy_sandbox::CanonicalTopic> fake_current_topics_ = {
      {browsing_topics::Topic(1), kFakeTaxonomyVersion},
      {browsing_topics::Topic(2), kFakeTaxonomyVersion}};
  std::set<privacy_sandbox::CanonicalTopic> fake_blocked_topics_ = {
      {browsing_topics::Topic(3), kFakeTaxonomyVersion},
      {browsing_topics::Topic(4), kFakeTaxonomyVersion}};

  // Informs the TrustSafetySentimentService, if it exists, that a
  // Privacy Sandbox interaction for an area has occurred The area is
  // determined by |action|. Only a subset of actions has a corresponding area.
  void InformSentimentService(PrivacySandboxService::PromptAction action);

  // Record user action metrics based on the |action|.
  void RecordPromptActionMetrics(PrivacySandboxService::PromptAction action);

  // Called when the Topics preference is changed.
  void OnTopicsPrefChanged();

  // Called when the Fledge preference is changed.
  void OnFledgePrefChanged();

  // Called when the Ad measurement preference is changed.
  void OnAdMeasurementPrefChanged();

  // Returns a PrivacySandboxCountries reference.
  PrivacySandboxCountries* GetPrivacySandboxCountries();

  // Returns true if _any_ of the k-API prefs are disabled via policy or
  // the prompt was suppressed via policy.
  static bool IsM1PrivacySandboxEffectivelyManaged(PrefService* pref_service);

  bool force_chrome_build_for_tests_ = false;

  base::WeakPtrFactory<PrivacySandboxServiceImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_
