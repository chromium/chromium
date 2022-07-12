// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

class Browser;
class PrefService;
#if !BUILDFLAG(IS_ANDROID)
class TrustSafetySentimentService;
#endif

namespace content {
class BrowsingDataRemover;
class InterestGroupManager;
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
class PrivacySandboxService : public KeyedService,
                              public policy::PolicyService::Observer,
                              public syncer::SyncServiceObserver,
                              public signin::IdentityManager::Observer {
 public:
  // Possible types of Privacy Sandbox prompts that may be shown to the user.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class PromptType {
    kNone = 0,
    kNotice = 1,
    kConsent = 2,
    kMaxValue = kConsent,
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

    kMaxValue = kNoticeLearnMore,
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
      browsing_topics::BrowsingTopicsService* browsing_topics_service);

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

  // Returns whether |url| is suitable to display the Privacy Sandbox dialog
  // over. Only about:blank and certain chrome:// URLs are considered suitable.
  static bool IsUrlSuitableForDialog(const GURL& url);

  // Functions for coordinating the display of the Privacy Sandbox dialog
  // across multiple browser windows. Only relevant for Desktop.

  // Informs the service that a Privacy Sandbox dialog |view| has been opened
  // or closed for |browser|.
  // Virtual to allow mocking in tests.
  virtual void DialogOpenedForBrowser(Browser* browser);
  virtual void DialogClosedForBrowser(Browser* browser);

  // Returns whether a Privacy Sandbox dialog is currently open for |browser|.
  // Virtual to allow mocking in tests.
  virtual bool IsDialogOpenForBrowser(Browser* browser);

  // Disables the display of the Privacy Sandbox dialog for testing. When
  // |disabled| is true, GetRequiredPromptType() will only ever return that no
  // dialog is required.
  // NOTE: This is set to true in InProcessBrowserTest::SetUp, disabling the
  // dialog for those tests. If you set this outside of that context, you should
  // ensure it is reset at the end of your test.
  static void SetDialogDisabledForTests(bool disabled);

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

 protected:
  friend class PrivacySandboxServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           MetricsLoggingOccursCorrectly);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestNonRegularProfile,
                           NoMetricsRecorded);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDialogTest, RestrictedDialog);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDialogTest, ManagedNoDialog);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDialogTest,
                           ManuallyControlledNoDialog);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDialogTest, NoParamNoDialog);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDeathTest,
                           GetRequiredPromptType);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxDialogNoticeWaiting);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxDialogConsentWaiting);
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
                           PrivacySandboxNoDialogDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           PrivacySandboxNoDialogEnabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest, PrivacySandboxRestricted);

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

  // Contains the possible states of a users Privacy Sandbox overall settings.
  // Must be kept in sync with SettingsPrivacySandboxStartupStates in
  // histograms/enums.xml
  enum class PSStartupStates {
    kDialogWaiting = 0,
    kDialogOffV1OffEnabled = 1,
    kDialogOffV1OffDisabled = 2,
    kConsentShownEnabled = 3,
    kConsentShownDisabled = 4,
    kNoticeShownEnabled = 5,
    kNoticeShownDisabled = 6,
    kDialogOff3PCOffEnabled = 7,
    kDialogOff3PCOffDisabled = 8,
    kDialogOffManagedEnabled = 9,
    kDialogOffManagedDisabled = 10,
    kDialogOffRestricted = 11,
    kDialogOffManuallyControlledEnabled = 12,
    kDialogOffManuallyControlledDisabled = 13,
    kNoDialogRequiredEnabled = 14,
    kNoDialogRequiredDisabled = 15,

    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kNoDialogRequiredDisabled,
  };

  // Helper function to actually make the metrics call for
  // LogPrivacySandboxState.
  void RecordPrivacySandboxHistogram(SettingsPrivacySandboxEnabled state);

  // Logs the state of the privacy sandbox and cookie settings. Called once per
  // profile startup.
  void LogPrivacySandboxState();

  // Logs the state of privacy sandbox 3 in regards to dialogs. Called once per
  // profile startup.
  void RecordPrivacySandbox3StartupMetrics();

  // Converts the provided list of |top_frames| into eTLD+1s for display, and
  // provides those to |callback|.
  void ConvertFledgeJoiningTopFramesForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<url::Origin> top_frames);

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

  PrefChangeRegistrar user_prefs_registrar_;

  // The set of Browser windows which have an open Privacy Sandbox dialog.
  std::set<Browser*> browsers_with_open_dialogs_;

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

  base::WeakPtrFactory<PrivacySandboxService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
