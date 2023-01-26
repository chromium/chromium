// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
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

// Service which encapsulates logic related to displaying and controlling the
// users Privacy Sandbox settings. This service contains the chrome/ specific
// logic used by the UI, including decision making around what the users'
// Privacy Sandbox settings should be based on their existing settings.
// Ultimately the decisions made by this service are consumed (through
// preferences and content settings) by the PrivacySandboxSettings located in
// components/privacy_sandbox/, which in turn makes them available to Privacy
// Sandbox APIs.
class PrivacySandboxService : public KeyedService {
 public:
  // Possible types of Privacy Sandbox prompts that may be shown to the user.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class PromptType {
    kNone = 0,
    kNotice = 1,
    kConsent = 2,
    kM1Consent = 3,
    kM1NoticeROW = 4,
    kM1NoticeEEA = 5,
    kMaxValue = kM1NoticeEEA,
  };

  // An exhaustive list of actions related to showing & interacting with the
  // prompt. Includes actions which do not impact consent / notice state.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class PromptAction {
    // Notice Interactions:
    kNoticeShown = 0,
    kNoticeOpenSettings = 1,
    kNoticeAcknowledge = 2,
    kNoticeDismiss = 3,

    // Implies that the browser, or browser window, was shut before the user
    // interacted with the notice.
    kNoticeClosedNoInteraction = 4,

    // Consent Interactions:
    kConsentShown = 5,
    kConsentAccepted = 6,
    kConsentDeclined = 7,
    kConsentMoreInfoOpened = 8,
    kConsentMoreInfoClosed = 9,

    // Implies that the browser, or browser window, was shut before the user
    // has made the decision (accepted or declined the consent).
    kConsentClosedNoDecision = 10,

    // Interaction with notice bubble: click on the link to open interests
    // settings.
    kNoticeLearnMore = 11,

    // Interactions with M1 Notice ROW prompt and M1 Notice EEA prompt.
    kNoticeMoreInfoOpened = 12,
    kNoticeMoreInfoClosed = 13,

    // The button is shown only when the prompt content isn't fully visible.
    kConsentMoreButtonClicked = 14,
    kNoticeMoreButtonClicked = 15,

    kMaxValue = kNoticeMoreButtonClicked,
  };

  // TODO(crbug.com/1378703): Integrate this when handling Notice and Consent
  // logic for m1.
  enum class PromptSuppressedReason {
    // Prompt has never been suppressed
    kNone = 0,
    // User had the Privacy Sandbox restricted at confirmation
    kRestricted = 1,
    // User was blocking 3PC when we attempted consent
    kThirdPartyCookiesBlocked = 2,
    // User declined the trials consent
    kTrialsConsentDeclined = 3,
    // User saw trials notice, and then disabled trials
    kTrialsDisabledAfterNotice = 4,
    // A policy is suppressing any prompt
    kPolicy = 5,
    // User migrated from EEA to ROW, and had already previously finished the
    // EEA consent flow.
    kEEAFlowCompletedBeforeRowMigration = 6,
    kMaxValue = kEEAFlowCompletedBeforeRowMigration,
  };

  PrivacySandboxService(
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
      first_party_sets::FirstPartySetsPolicyService* first_party_sets_service);

  ~PrivacySandboxService() override;

  // Returns the prompt type that should be shown to the user. This consults
  // previous consent / notice information stored in preferences, the current
  // state of the Privacy Sandbox settings, and the current location of the
  // user, to determine the appropriate type. This is expected to be called by
  // UI code locations determining whether a prompt should be shown on startup.
  // Virtual to allow mocking in tests.
  virtual PromptType GetRequiredPromptType();

  // Informs the service that |action| occurred with the prompt. This allows
  // the service to record this information in preferences such that future
  // calls to GetRequiredPromptType() are correct. This is expected to be
  // called appropriately by all locations showing the prompt. Metrics shared
  // between platforms will also be recorded.
  // This method is virtual for mocking in tests.
  virtual void PromptActionOccurred(PromptAction action);

  // Returns whether |url| is suitable to display the Privacy Sandbox prompt
  // over. Only about:blank and certain chrome:// URLs are considered suitable.
  static bool IsUrlSuitableForPrompt(const GURL& url);

  // Functions for coordinating the display of the Privacy Sandbox prompts
  // across multiple browser windows. Only relevant for Desktop.

  // Informs the service that a Privacy Sandbox prompt has been opened
  // or closed for |browser|.
  // Virtual to allow mocking in tests.
  virtual void PromptOpenedForBrowser(Browser* browser);
  virtual void PromptClosedForBrowser(Browser* browser);

  // Returns whether a Privacy Sandbox prompt is currently open for |browser|.
  // Virtual to allow mocking in tests.
  virtual bool IsPromptOpenForBrowser(Browser* browser);

  // Disables the display of the Privacy Sandbox prompt for testing. When
  // |disabled| is true, GetRequiredPromptType() will only ever return that no
  // prompt is required.
  // NOTE: This is set to true in InProcessBrowserTest::SetUp, disabling the
  // prompt for those tests. If you set this outside of that context, you should
  // ensure it is reset at the end of your test.
  static void SetPromptDisabledForTests(bool disabled);

  // If set to true, this treats the testing environment as that of a branded
  // Chrome build.
  void ForceChromeBuildForTests(bool force_chrome_build);

  // Disables the Privacy Sandbox completely if |enabled| is false. If |enabled|
  // is true, context specific as well as restriction checks will still be
  // performed to determine if specific APIs are available in specific contexts.
  void SetPrivacySandboxEnabled(bool enabled);

  // Used by the UI to check if the API is enabled. This is a UI function ONLY.
  // Checks the primary pref directly, and _only_ the primary pref. There are
  // many other reasons that API access may be denied that are not checked by
  // this function. All decisions for allowing access to APIs should be routed
  // through the PrivacySandboxSettings class.
  // TODO(crbug.com/1310157): Rename this function to better reflect this.
  bool IsPrivacySandboxEnabled();

  // Returns whether the state of the API is managed.
  bool IsPrivacySandboxManaged();

  // Returns whether the Privacy Sandbox is currently restricted for the
  // profile. UI code should consult this to ensure that when restricted,
  // Privacy Sandbox related UI is updated appropriately.
  virtual bool IsPrivacySandboxRestricted();

  // Called when the V2 Privacy Sandbox preference is changed.
  void OnPrivacySandboxV2PrefChanged();

  // Returns whether the FirstPartySets preference is enabled.
  bool IsFirstPartySetsDataAccessEnabled() const;

  // Returns whether the FirstPartySets preference is managed.
  virtual bool IsFirstPartySetsDataAccessManaged() const;

  // Toggles the FirstPartySets preference.
  void SetFirstPartySetsDataAccessEnabled(bool enabled);

  // Returns the set of eTLD + 1's on which the user was joined to a FLEDGE
  // interest group. Consults with the InterestGroupManager associated with
  // |profile_| and formats the returned data for direct display to the user.
  // Virtual to allow mocking in tests.
  virtual void GetFledgeJoiningEtldPlusOneForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Returns the set of top frames which are blocked from joining the profile to
  // an interest group. Virtual to allow mocking in tests.
  virtual std::vector<std::string> GetBlockedFledgeJoiningTopFramesForDisplay()
      const;

  // Sets Fledge interest group joining to |allowed| for |top_frame_etld_plus1|.
  // Forwards the setting to the PrivacySandboxSettings service, but also
  // removes any Fledge data for the |top_frame_etld_plus1| if |allowed| is
  // false.
  // Virtual to allow mocking in tests.
  virtual void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                                       bool allowed) const;

  // Returns the top topics for the previous N epochs.
  // Virtual for mocking in tests.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetCurrentTopTopics()
      const;

  // Returns the set of topics which have been blocked by the user.
  // Virtual for mocking in tests.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetBlockedTopics() const;

  // Sets a |topic_id|, as both a top topic and topic provided to the web, to be
  // allowed/blocked based on the value of |allowed|. This is stored to
  // preferences and made available to the Topics API via the
  // PrivacySandboxSettings class. This function expects that |topic| will have
  // previously been provided by one of the above functions. Virtual for mocking
  // in tests.
  virtual void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                               bool allowed);

  // DEPRECATED - Do not use in new code. It will be replaced with queries to
  // the First-Party Sets that are in the browser-process.
  // Virtual for mocking in tests.
  virtual base::flat_map<net::SchemefulSite, net::SchemefulSite>
  GetSampleFirstPartySets() const;

  // Returns the owner domain of the first party set that `site_url` is a member
  // of, or absl::nullopt if `site_url` is not recognised as a member of an FPS.
  // Encapsulates logic about whether FPS information should be shown, if it
  // should not, absl::nullopt is always returned.
  // Virtual for mocking in tests.
  virtual absl::optional<net::SchemefulSite> GetFirstPartySetOwner(
      const GURL& site_url) const;

  // Same as GetFirstPartySetOwner but returns a formatted string.
  virtual absl::optional<std::u16string> GetFirstPartySetOwnerForDisplay(
      const GURL& site_url) const;

  // Returns true if `site`'s membership in an FPS is being managed by policy or
  // if FirstPartySets preference is managed. Virtual for mocking in tests.
  //
  // Note: Enterprises can use the First-Party Set Overrides policy to either
  // add or remove a site from a First-Party Set. This method returns true only
  // if `site` is being added into a First-Party Set since there's no UI use for
  // whether `site` is being removed by an enterprise yet.
  virtual bool IsPartOfManagedFirstPartySet(
      const net::SchemefulSite& site) const;

  // Inform the service that the user changed the Topics toggle in settings,
  // so that the current topics consent information can be updated.
  // TODO (crbug.com/1378703): Determine whether changes to the preference,
  // such as by policy or extensions, should also call here.
  // Virtual for mocking in tests.
  virtual void TopicsToggleChanged(bool new_value) const;

  // Whether the current profile requires consent for Topics to operate.
  bool TopicsConsentRequired() const;

  // Whether there is an active consent for Topics currently recorded.
  bool TopicsHasActiveConsent() const;

  // Functions which returns the details of the currently recorded Topics
  // consent.
  // TODO (crbug.com/1378703): Display the output of these functions in WebUI.
  privacy_sandbox::TopicsConsentUpdateSource TopicsConsentLastUpdateSource()
      const;
  base::Time TopicsConsentLastUpdateTime() const;
  std::string TopicsConsentLastUpdateText() const;

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
      PrivacySandboxServiceM1Test,
      RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Explicitly);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1Test,
      RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Implicitly);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1Test,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_EEA);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxServiceM1Test,
      RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_ROW);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceM1Test,
                           RecordPrivacySandbox4StartupMetrics_APIs);

  // Should be used only for tests when mocking the service.
  PrivacySandboxService();

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

    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kPromptNotShownDueToManagedState,
  };

  // Helper function to log first party sets state.
  void RecordFirstPartySetsStateHistogram(FirstPartySetsState state);

  // Helper function to actually make the metrics call for
  // LogPrivacySandboxState.
  void RecordPrivacySandboxHistogram(SettingsPrivacySandboxEnabled state);

  // Logs the state of the privacy sandbox and cookie settings. Called once per
  // profile startup.
  void LogPrivacySandboxState();

  // Logs the state of privacy sandbox 3 in regards to prompts. Called once per
  // profile startup.
  void RecordPrivacySandbox3StartupMetrics();

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
      bool third_party_cookies_blocked);

  // Equivalent of PrivacySandboxService::GetRequiredPromptTypeInternal, but for
  // PrivacySandboxSettings4.
  static PrivacySandboxService::PromptType GetRequiredPromptTypeInternalM1(
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

 private:
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<content::InterestGroupManager> interest_group_manager_;
  profile_metrics::BrowserProfileType profile_type_;
  raw_ptr<content::BrowsingDataRemover> browsing_data_remover_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<TrustSafetySentimentService> sentiment_service_;
#endif
  raw_ptr<browsing_topics::BrowsingTopicsService> browsing_topics_service_;
  raw_ptr<first_party_sets::FirstPartySetsPolicyService>
      first_party_sets_policy_service_;

  PrefChangeRegistrar user_prefs_registrar_;

  // The set of Browser windows which have an open Privacy Sandbox prompt.
  std::set<Browser*> browsers_with_open_prompts_;

  // Fake implementation for current and blocked topics.
  std::set<privacy_sandbox::CanonicalTopic> fake_current_topics_ = {
      {browsing_topics::Topic(1),
       privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY},
      {browsing_topics::Topic(2),
       privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY}};
  std::set<privacy_sandbox::CanonicalTopic> fake_blocked_topics_ = {
      {browsing_topics::Topic(3),
       privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY},
      {browsing_topics::Topic(4),
       privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY}};

  // Informs the TrustSafetySentimentService, if it exists, that a
  // Privacy Sandbox 3 interaction for an area has occurred The area is
  // determined by |action|. Only a subset of actions has a corresponding area.
  void InformSentimentService(PrivacySandboxService::PromptAction action);

  // Equivalent of PrivacySandboxService::InformSentimentService, but for
  // PrivacySandboxSettings4.
  void InformSentimentServiceM1(PrivacySandboxService::PromptAction action);

  // Implementation of PrivacySandboxService::PromptActionOccurred, but for
  // PrivacySandboxSettings4.
  virtual void PromptActionOccurredM1(PromptAction action);

  // Record user action metrics based on the |action|.
  void RecordPromptActionMetrics(PrivacySandboxService::PromptAction action);

  // Called when the Topics preference is changed.
  void OnTopicsPrefChanged();

  // Called when the Fledge preference is changed.
  void OnFledgePrefChanged();

  // Called when the Ad measurement preference is changed.
  void OnAdMeasurementPrefChanged();

  // Returns true if _any_ of the k-API prefs are disabled via policy or
  // the prompt was suppressed via policy.
  static bool IsM1PrivacySandboxEffectivelyManaged(PrefService* pref_service);

  bool force_chrome_build_for_tests_ = false;

  base::WeakPtrFactory<PrivacySandboxService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
