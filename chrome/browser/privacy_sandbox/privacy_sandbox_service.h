// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/interest_group_manager.h"
#include "net/base/schemeful_site.h"

class Browser;

namespace views {
class Widget;
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
    kM1NoticeRestricted = 6,
    kMaxValue = kM1NoticeRestricted,
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

    // Restricted notice interactions
    kRestrictedNoticeAcknowledge = 16,
    kRestrictedNoticeOpenSettings = 17,
    kRestrictedNoticeShown = 18,
    kRestrictedNoticeClosedNoInteraction = 19,
    kRestrictedNoticeMoreButtonClicked = 20,

    kMaxValue = kRestrictedNoticeMoreButtonClicked,
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
    // User migrated from ROW to EEA, but had already disabled Topics from
    // settings.
    kROWFlowCompletedAndTopicsDisabledBeforeEEAMigration = 7,
    // The user is restricted with a guardian, so a direct notice is shown.
    kNoticeShownToGuardian = 8,
    kMaxValue = kNoticeShownToGuardian,
  };

  // Returns whether |url| is suitable to display the Privacy Sandbox prompt
  // over. Only about:blank and certain chrome:// URLs are considered suitable.
  static bool IsUrlSuitableForPrompt(const GURL& url);

  // Disables the display of the Privacy Sandbox prompt for testing. When
  // |disabled| is true, GetRequiredPromptType() will only ever return that no
  // prompt is required.
  // NOTE: This is set to true in InProcessBrowserTest::SetUp, disabling the
  // prompt for those tests. If you set this outside of that context, you should
  // ensure it is reset at the end of your test.
  static void SetPromptDisabledForTests(bool disabled);

  // Returns the prompt type that should be shown to the user. This consults
  // previous consent / notice information stored in preferences, the current
  // state of the Privacy Sandbox settings, and the current location of the
  // user, to determine the appropriate type. This is expected to be called by
  // UI code locations determining whether a prompt should be shown on startup.
  virtual PromptType GetRequiredPromptType() = 0;

  // Informs the service that |action| occurred with the prompt. This allows
  // the service to record this information in preferences such that future
  // calls to GetRequiredPromptType() are correct. This is expected to be
  // called appropriately by all locations showing the prompt. Metrics shared
  // between platforms will also be recorded.
  virtual void PromptActionOccurred(PromptAction action) = 0;

  // Functions for coordinating the display of the Privacy Sandbox prompts
  // across multiple browser windows. Only relevant for Desktop.

#if !BUILDFLAG(IS_ANDROID)
  // Informs the service that a Privacy Sandbox prompt has been opened
  // or closed for |browser|.
  virtual void PromptOpenedForBrowser(Browser* browser,
                                      views::Widget* widget) = 0;
  virtual void PromptClosedForBrowser(Browser* browser) = 0;

  // Returns whether a Privacy Sandbox prompt is currently open for |browser|.
  virtual bool IsPromptOpenForBrowser(Browser* browser) = 0;
#endif  // !BUILDFLAG(IS_ANDROID)

  // If set to true, this treats the testing environment as that of a branded
  // Chrome build.
  virtual void ForceChromeBuildForTests(bool force_chrome_build) = 0;

  // Disables the Privacy Sandbox completely if |enabled| is false. If |enabled|
  // is true, context specific as well as restriction checks will still be
  // performed to determine if specific APIs are available in specific contexts.
  virtual void SetPrivacySandboxEnabled(bool enabled) = 0;

  // Used by the UI to check if the API is enabled. This is a UI function ONLY.
  // Checks the primary pref directly, and _only_ the primary pref. There are
  // many other reasons that API access may be denied that are not checked by
  // this function. All decisions for allowing access to APIs should be routed
  // through the PrivacySandboxSettings class.
  // TODO(crbug.com/1310157): Rename this function to better reflect this.
  virtual bool IsPrivacySandboxEnabled() = 0;

  // Returns whether the state of the API is managed.
  virtual bool IsPrivacySandboxManaged() = 0;

  // Returns whether the Privacy Sandbox is currently restricted for the
  // profile. UI code should consult this to ensure that when restricted,
  // Privacy Sandbox related UI is updated appropriately.
  virtual bool IsPrivacySandboxRestricted() = 0;

  // Called when the V2 Privacy Sandbox preference is changed.
  virtual void OnPrivacySandboxV2PrefChanged() = 0;

  // Returns whether the Privacy Sandbox is configured to show a restricted
  // notice.
  virtual bool IsRestrictedNoticeEnabled() = 0;

  // Toggles the RelatedWebsiteSets preference.
  virtual void SetFirstPartySetsDataAccessEnabled(bool enabled) = 0;

  // Returns whether the RelatedWebsiteSets preference is enabled.
  virtual bool IsFirstPartySetsDataAccessEnabled() const = 0;

  // Returns whether the RelatedWebsiteSets preference is managed.
  virtual bool IsFirstPartySetsDataAccessManaged() const = 0;

  // DEPRECATED - Do not use in new code. It will be replaced with queries to
  // the First-Party Sets that are in the browser-process.
  // Virtual for mocking in tests.
  virtual base::flat_map<net::SchemefulSite, net::SchemefulSite>
  GetSampleFirstPartySets() const = 0;

  // Returns the owner domain of the first party set that `site_url` is a member
  // of, or absl::nullopt if `site_url` is not recognised as a member of an FPS.
  // Encapsulates logic about whether FPS information should be shown, if it
  // should not, absl::nullopt is always returned.
  // Virtual for mocking in tests.
  virtual absl::optional<net::SchemefulSite> GetFirstPartySetOwner(
      const GURL& site_url) const = 0;

  // Same as GetFirstPartySetOwner but returns a formatted string.
  virtual absl::optional<std::u16string> GetFirstPartySetOwnerForDisplay(
      const GURL& site_url) const = 0;

  // Returns true if `site`'s membership in an FPS is being managed by policy or
  // if FirstPartySets preference is managed. Virtual for mocking in tests.
  //
  // Note: Enterprises can use the First-Party Set Overrides policy to either
  // add or remove a site from a First-Party Set. This method returns true only
  // if `site` is being added into a First-Party Set since there's no UI use for
  // whether `site` is being removed by an enterprise yet.
  virtual bool IsPartOfManagedFirstPartySet(
      const net::SchemefulSite& site) const = 0;

  // Returns the set of eTLD + 1's on which the user was joined to a FLEDGE
  // interest group. Consults with the InterestGroupManager associated with
  // |profile_| and formats the returned data for direct display to the user.
  virtual void GetFledgeJoiningEtldPlusOneForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback) = 0;

  // Returns the set of top frames which are blocked from joining the profile to
  // an interest group.
  virtual std::vector<std::string> GetBlockedFledgeJoiningTopFramesForDisplay()
      const = 0;

  // Sets Fledge interest group joining to |allowed| for |top_frame_etld_plus1|.
  // Forwards the setting to the PrivacySandboxSettings service, but also
  // removes any Fledge data for the |top_frame_etld_plus1| if |allowed| is
  // false.
  virtual void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                                       bool allowed) const = 0;

  // Returns the top topics for the previous N epochs.
  // Virtual for mocking in tests.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetCurrentTopTopics()
      const = 0;

  // Returns the set of topics which have been blocked by the user.
  // Virtual for mocking in tests.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetBlockedTopics()
      const = 0;

  // Sets a |topic_id|, as both a top topic and topic provided to the web, to be
  // allowed/blocked based on the value of |allowed|. This is stored to
  // preferences and made available to the Topics API via the
  // PrivacySandboxSettings class. This function expects that |topic| will have
  // previously been provided by one of the above functions. Virtual for mocking
  // in tests.
  virtual void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                               bool allowed) = 0;

  // Inform the service that the user changed the Topics toggle in settings,
  // so that the current topics consent information can be updated.
  // TODO (crbug.com/1378703): Determine whether changes to the preference,
  // such as by policy or extensions, should also call here.
  // Virtual for mocking in tests.
  virtual void TopicsToggleChanged(bool new_value) const = 0;

  // Whether the current profile requires consent for Topics to operate.
  virtual bool TopicsConsentRequired() const = 0;

  // Whether there is an active consent for Topics currently recorded.
  virtual bool TopicsHasActiveConsent() const = 0;

  // Functions which returns the details of the currently recorded Topics
  // consent.
  // TODO (crbug.com/1378703): Display the output of these functions in WebUI.
  virtual privacy_sandbox::TopicsConsentUpdateSource
  TopicsConsentLastUpdateSource() const = 0;
  virtual base::Time TopicsConsentLastUpdateTime() const = 0;
  virtual std::string TopicsConsentLastUpdateText() const = 0;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
