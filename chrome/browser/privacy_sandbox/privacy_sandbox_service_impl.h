// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_

// clang-format off
#include "chrome/browser/privacy_sandbox/notice/notice_definitions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
// clang-format on

#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/interest_group_manager.h"
#include "net/base/schemeful_site.h"

class PrefService;

namespace content {
class BrowsingDataRemover;
}

namespace content_settings {
class CookieSettings;
}

namespace browsing_topics {
class BrowsingTopicsService;
}

class PrivacySandboxServiceImpl : public PrivacySandboxService {
 public:
  PrivacySandboxServiceImpl(
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
      PrivacySandboxCountries* privacy_sandbox_countries);

  ~PrivacySandboxServiceImpl() override;

  // KeyedService:
  void Shutdown() override;

  // PrivacySandboxService:
  void ForceChromeBuildForTests(bool force_chrome_build) override;
  bool IsPrivacySandboxRestricted() override;
  bool IsRestrictedNoticeEnabled() override;
  void SetRelatedWebsiteSetsDataAccessEnabled(bool enabled) override;
  bool IsRelatedWebsiteSetsDataAccessEnabled() const override;
  bool IsRelatedWebsiteSetsDataAccessManaged() const override;
  std::optional<net::SchemefulSite> GetRelatedWebsiteSetOwner(
      const GURL& site_url) const override;
  std::optional<std::u16string> GetRelatedWebsiteSetOwnerForDisplay(
      const GURL& site_url) const override;
  bool IsPartOfManagedRelatedWebsiteSet(
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
  bool ShouldUsePrivacyPolicyChinaDomain() override;
  void TopicsToggleChanged(bool new_value) const override;
  bool TopicsConsentRequired() override;
  bool TopicsHasActiveConsent() const override;
  privacy_sandbox::TopicsConsentUpdateSource TopicsConsentLastUpdateSource()
      const override;
  base::Time TopicsConsentLastUpdateTime() const override;
  std::string TopicsConsentLastUpdateText() const override;
  void UpdateTopicsApiResult(bool value) override;
  void UpdateProtectedAudienceApiResult(bool value) override;
  void UpdateMeasurementApiResult(bool value) override;
  privacy_sandbox::EligibilityLevel GetTopicsApiEligibility() override;
  privacy_sandbox::EligibilityLevel GetProtectedAudienceApiEligibility()
      override;
  privacy_sandbox::EligibilityLevel GetAdMeasurementApiEligibility() override;

 protected:
  friend class PrivacySandboxServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsNotRelevantMetricAllowedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsNotRelevantMetricBlockedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsEnabledMetric);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsDisabledMetric);
  FRIEND_TEST_ALL_PREFIXES(LogPrivacySandboxStateNonRegularProfilesTest, APIs);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           LogPrivacySandboxState_APIs);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
                           ReturnsCorrectStatus);

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

  // Helper function to log first party sets state.
  void RecordFirstPartySetsStateHistogram();

  // Helper function to log tracking protection state.
  void RecordTrackingProtectionStateHistogram();

  // Logs the state of the Privacy Sandbox APIs (Topics, Protected Audience,
  // Ad Measurement) and cookie-related settings (FPS, Tracking Protection).
  // Called once per profile startup.
  void LogPrivacySandboxState();

  // Converts the provided list of |top_frames| into eTLD+1s for display, and
  // provides those to |callback|.
  void ConvertInterestGroupDataKeysForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<content::InterestGroupManager::InterestGroupDataKey>
          data_keys);

  // Checks to see if initialization of the user's RWS pref is required, and if
  // so, sets the default value based on the user's current cookie settings.
  void MaybeInitializeRelatedWebsiteSetsPref();

  // Updates the preferences which store the current Topics consent information.
  void RecordUpdatedTopicsConsent(
      privacy_sandbox::TopicsConsentUpdateSource source,
      bool did_consent) const;

 private:
  // Determines whether Privacy Sandbox Ads consent is required.
  bool IsConsentRequired();
  // Determines whether a Privacy Sandbox Ads notice is required.
  bool IsNoticeRequired();
  // Determines whether the Privacy Sandbox Ads Restricted notice is required.
  bool IsRestrictedNoticeRequired();

  raw_ptr<Profile> profile_;
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<content::InterestGroupManager> interest_group_manager_;
  profile_metrics::BrowserProfileType profile_type_;
  raw_ptr<content::BrowsingDataRemover> browsing_data_remover_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  raw_ptr<browsing_topics::BrowsingTopicsService> browsing_topics_service_;
  raw_ptr<first_party_sets::FirstPartySetsPolicyService>
      first_party_sets_policy_service_;
  raw_ptr<PrivacySandboxCountries> privacy_sandbox_countries_;

  PrefChangeRegistrar user_prefs_registrar_;

  // Fake implementation for current and blocked topics.
  // TODO(crbug.com/409048902): Moved initialization to constructor to prevent
  // potential initialization order issues.
  std::set<privacy_sandbox::CanonicalTopic> fake_current_topics_;
  std::set<privacy_sandbox::CanonicalTopic> fake_blocked_topics_;

  // Called when the Topics preference is changed.
  void OnTopicsPrefChanged();

  // Called when the Fledge preference is changed.
  void OnFledgePrefChanged();

  // Called when the Ad measurement preference is changed.
  void OnAdMeasurementPrefChanged();

  // Returns a PrivacySandboxCountries reference.
  PrivacySandboxCountries* GetPrivacySandboxCountries();

  bool force_chrome_build_for_tests_ = false;

  base::WeakPtrFactory<PrivacySandboxServiceImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_
