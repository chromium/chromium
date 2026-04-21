// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/privacy_sandbox/notice/notice_definitions.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "net/base/schemeful_site.h"

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
  // If set to true, this treats the testing environment as that of a branded
  // Chrome build.
  virtual void ForceChromeBuildForTests(bool force_chrome_build) = 0;

  // Returns whether the Privacy Sandbox is currently restricted for the
  // profile. UI code should consult this to ensure that when restricted,
  // Privacy Sandbox related UI is updated appropriately.
  virtual bool IsPrivacySandboxRestricted() = 0;

  // Returns whether the Privacy Sandbox is configured to show a restricted
  // notice.
  virtual bool IsRestrictedNoticeEnabled() = 0;

  // Toggles the RelatedWebsiteSets preference.
  virtual void SetRelatedWebsiteSetsDataAccessEnabled(bool enabled) = 0;

  // Returns whether the RelatedWebsiteSets preference is enabled.
  virtual bool IsRelatedWebsiteSetsDataAccessEnabled() const = 0;

  // Returns whether the RelatedWebsiteSets preference is managed.
  virtual bool IsRelatedWebsiteSetsDataAccessManaged() const = 0;

  // Returns the owner domain of the related website set that `site_url` is a
  // member of, or std::nullopt if `site_url` is not recognised as a member of
  // an RWS. Encapsulates logic about whether RWS information should be shown,
  // if it should not, std::nullopt is always returned. Virtual for mocking in
  // tests.
  virtual std::optional<net::SchemefulSite> GetRelatedWebsiteSetOwner(
      const GURL& site_url) const = 0;

  // Same as GetRelatedWebsiteSetOwner but returns a formatted string.
  virtual std::optional<std::u16string> GetRelatedWebsiteSetOwnerForDisplay(
      const GURL& site_url) const = 0;

  // Returns true if `site`'s membership in an RWS is being managed by policy or
  // if RelatedWebsiteSets preference is managed. Virtual for mocking in tests.
  //
  // Note: Enterprises can use the Related Website Set Overrides policy to
  // either add or remove a site from a Related Website Set. This method returns
  // true only if `site` is being added into a Related Website Set since there's
  // no UI use for whether `site` is being removed by an enterprise yet.
  virtual bool IsPartOfManagedRelatedWebsiteSet(
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

  // Returns the first level topic: they are the root topics, meaning that they
  // have no parent.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetFirstLevelTopics()
      const = 0;

  // Returns the list of assigned children topics (direct or indirect) of the
  // passed-in topic.
  virtual std::vector<privacy_sandbox::CanonicalTopic>
  GetChildTopicsCurrentlyAssigned(
      const privacy_sandbox::CanonicalTopic& topic) const = 0;

  // Sets a |topic_id|, as both a top topic and topic provided to the web, to be
  // allowed/blocked based on the value of |allowed|. This is stored to
  // preferences and made available to the Topics API via the
  // PrivacySandboxSettings class. This function expects that |topic| will have
  // previously been provided by one of the above functions. Virtual for mocking
  // in tests.
  virtual void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                               bool allowed) = 0;

  // Determines whether the Topics API step should be shown in the Privacy
  // Guide.
  virtual bool PrivacySandboxPrivacyGuideShouldShowAdTopicsCard() = 0;

  // Determines whether the China domain should be used for the Privacy Policy
  // page.
  virtual bool ShouldUsePrivacyPolicyChinaDomain() = 0;

  // Inform the service that the user changed the Topics toggle in settings,
  // so that the current topics consent information can be updated.
  // This is not fired for changes to the preference for policy or extensions,
  // and so consent information only represents direct user actions. Note that
  // extensions and policy can only _disable_ topics, and so cannot bypass the
  // need for user consent where required.
  // Virtual for mocking in tests.
  virtual void TopicsToggleChanged(bool new_value) const = 0;

  // Whether the current profile requires consent for Topics to operate.
  virtual bool TopicsConsentRequired() = 0;

  // Whether there is an active consent for Topics currently recorded.
  virtual bool TopicsHasActiveConsent() const = 0;

  // Functions which returns the details of the currently recorded Topics
  // consent.
  virtual privacy_sandbox::TopicsConsentUpdateSource
  TopicsConsentLastUpdateSource() const = 0;
  virtual base::Time TopicsConsentLastUpdateTime() const = 0;
  virtual std::string TopicsConsentLastUpdateText() const = 0;

  // Notice Framework Result Callbacks.
  virtual void UpdateTopicsApiResult(bool value) = 0;
  virtual void UpdateProtectedAudienceApiResult(bool value) = 0;
  virtual void UpdateMeasurementApiResult(bool value) = 0;
  // Notice Framework Eligibility Callbacks.
  virtual privacy_sandbox::EligibilityLevel GetTopicsApiEligibility() = 0;
  virtual privacy_sandbox::EligibilityLevel
  GetProtectedAudienceApiEligibility() = 0;
  virtual privacy_sandbox::EligibilityLevel
  GetAdMeasurementApiEligibility() = 0;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
