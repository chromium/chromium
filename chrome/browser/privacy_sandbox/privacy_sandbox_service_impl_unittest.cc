// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <tuple>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_topics/test_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/privacy_sandbox/mock_privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/login_state/scoped_test_public_session_login_state.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace {
using ::browsing_topics::Topic;
using ::privacy_sandbox::CanonicalTopic;
using ::testing::ElementsAre;
using PromptAction = ::PrivacySandboxService::PromptAction;
using PromptSuppressedReason = ::PrivacySandboxService::PromptSuppressedReason;
using PromptType = ::PrivacySandboxService::PromptType;
using SurfaceType = ::PrivacySandboxService::SurfaceType;

#if BUILDFLAG(IS_ANDROID)
using ActivityType = PrivacySandboxService::PrivacySandboxStorageActivityType;
using UserSegment =
    PrivacySandboxService::PrivacySandboxStorageUserSegmentByRecentActivity;
#endif  // BUILDFLAG(IS_ANDROID)

using enum privacy_sandbox_test_util::StateKey;
using enum privacy_sandbox_test_util::InputKey;
using enum privacy_sandbox_test_util::OutputKey;

using privacy_sandbox_test_util::InputKey;
using privacy_sandbox_test_util::OutputKey;
using privacy_sandbox_test_util::StateKey;

using privacy_sandbox_test_util::MultipleInputKeys;
using privacy_sandbox_test_util::MultipleOutputKeys;
using privacy_sandbox_test_util::MultipleStateKeys;
using privacy_sandbox_test_util::SiteDataExceptions;
using privacy_sandbox_test_util::TestCase;
using privacy_sandbox_test_util::TestInput;
using privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::TestState;

const char kFirstPartySetsStateHistogram[] = "Settings.FirstPartySets.State";

const base::Version kFirstPartySetsVersion("1.2.3");

constexpr int kTestTaxonomyVersion = 1;

class TestPrivacySandboxService
    : public privacy_sandbox_test_util::PrivacySandboxServiceTestInterface {
 public:
  explicit TestPrivacySandboxService(PrivacySandboxService* service)
      : service_(service) {}

  // PrivacySandboxServiceTestInterface
  void TopicsToggleChanged(bool new_value) const override {
    service_->TopicsToggleChanged(new_value);
  }
  void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                       bool allowed) override {
    service_->SetTopicAllowed(topic, allowed);
  }
  bool TopicsHasActiveConsent() const override {
    return service_->TopicsHasActiveConsent();
  }
  privacy_sandbox::TopicsConsentUpdateSource TopicsConsentLastUpdateSource()
      const override {
    return service_->TopicsConsentLastUpdateSource();
  }
  base::Time TopicsConsentLastUpdateTime() const override {
    return service_->TopicsConsentLastUpdateTime();
  }
  std::string TopicsConsentLastUpdateText() const override {
    return service_->TopicsConsentLastUpdateText();
  }
  void ForceChromeBuildForTests(bool force_chrome_build) const override {
    service_->ForceChromeBuildForTests(force_chrome_build);
  }
  int GetRequiredPromptType(int surface_type) const override {
    return static_cast<int>(service_->GetRequiredPromptType(
        static_cast<SurfaceType>(surface_type)));
  }
  void PromptActionOccurred(int action, int surface_type) const override {
    service_->PromptActionOccurred(static_cast<PromptAction>(action),
                                   static_cast<SurfaceType>(surface_type));
  }

 private:
  raw_ptr<PrivacySandboxService> service_;
};

class TestInterestGroupManager : public content::InterestGroupManager {
 public:
  void SetInterestGroupDataKeys(
      const std::vector<InterestGroupDataKey>& data_keys) {
    data_keys_ = data_keys;
  }

  // content::InterestGroupManager:
  void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  void GetAllInterestGroupDataKeys(
      base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback)
      override {
    std::move(callback).Run(data_keys_);
  }
  void RemoveInterestGroupsByDataKey(InterestGroupDataKey data_key,
                                     base::OnceClosure callback) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  std::vector<InterestGroupDataKey> data_keys_;
};

// Remove any user preference settings for First Party Set related preferences,
// returning them to their default value.
void ClearFpsUserPrefs(
    sync_preferences::TestingPrefServiceSyncable* pref_service) {
  pref_service->RemoveUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
  pref_service->RemoveUserPref(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized);
}

std::vector<int> GetTopicsSettingsStringIdentifiers(bool did_consent,
                                                    bool has_current_topics,
                                                    bool has_blocked_topics) {
  if (did_consent && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (did_consent && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && has_current_topics && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && has_current_topics && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && !has_current_topics && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && !has_current_topics && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_1,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_2,
            IDS_SETTINGS_TOPICS_PAGE_LEARN_MORE_BULLET_3_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  }

  NOTREACHED_IN_MIGRATION() << "Invalid topics settings consent state";
  return {};
}

std::vector<int> GetTopicsConfirmationStringIdentifiers() {
  return {IDS_PRIVACY_SANDBOX_M1_CONSENT_TITLE,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_1,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_2,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_3,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_DESCRIPTION_4,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_EXPAND_LABEL,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_1,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_2,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_BULLET_3,
          IDS_PRIVACY_SANDBOX_M1_CONSENT_LEARN_MORE_LINK};
}

struct NoticeTestingParameters {
  // Inputs
  SurfaceType surface_type;
  base::test::FeatureRefAndParams feature_flag;
  PromptAction shown_type;
  PromptAction prompt_action;
  // Expected Output
  // Represents the correct notice preference name that should be logged
  std::string_view notice_name;
};
}  // namespace

// A mock implementation of the PrivacySandboxCountries interface for testing.
class MockPrivacySandboxCountries : public PrivacySandboxCountries {
 public:
  MOCK_METHOD(bool, IsConsentCountry, (), (override));
  MOCK_METHOD(bool, IsRestOfWorldCountry, (), (override));
};

class PrivacySandboxServiceTest : public testing::Test {
 public:
  PrivacySandboxServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_attestations_(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting()) {
    notice_storage_ =
        std::make_unique<privacy_sandbox::PrivacySandboxNoticeStorage>();
  }

  void SetUp() override {
    InitializeFeaturesBeforeStart();
    CreateService();

    base::RunLoop run_loop;
    first_party_sets_policy_service_.WaitForFirstInitCompleteForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
    first_party_sets_policy_service_.ResetForTesting();
  }

  virtual void InitializeFeaturesBeforeStart() {}

  virtual std::unique_ptr<
      privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    return mock_delegate;
  }

  void CreateService() {
    // `CreateService` is sometimes called twice, or more in a tests.
    // Previous instances must be destroyed in the opposite order of their
    // construction.
    privacy_sandbox_service_.reset();

    auto mock_delegate = CreateMockDelegate();
    mock_delegate_ = mock_delegate.get();
    mock_privacy_sandbox_countries_ =
        std::make_unique<MockPrivacySandboxCountries>();

    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettingsImpl>(
            std::move(mock_delegate), host_content_settings_map(),
            cookie_settings(), tracking_protection_settings(), prefs());
#if !BUILDFLAG(IS_ANDROID)
    mock_sentiment_service_ =
        std::make_unique<::testing::NiceMock<MockTrustSafetySentimentService>>(
            profile());
#endif
    privacy_sandbox_service_ = std::make_unique<PrivacySandboxServiceImpl>(
        privacy_sandbox_settings(), tracking_protection_settings(),
        cookie_settings(), profile()->GetPrefs(), test_interest_group_manager(),
        GetProfileType(), browsing_data_remover(), host_content_settings_map(),
#if !BUILDFLAG(IS_ANDROID)
        mock_sentiment_service(),
#endif
        mock_browsing_topics_service(), first_party_sets_policy_service(),
        mock_privacy_sandbox_countries());
  }

  virtual profile_metrics::BrowserProfileType GetProfileType() {
    return profile_metrics::BrowserProfileType::kRegular;
  }

  void RunTestCase(const TestState& test_state,
                   const TestInput& test_input,
                   const TestOutput& test_output) {
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    auto* user_provider_raw = user_provider.get();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();
    auto* managed_provider_raw = managed_provider.get();
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(user_provider),
        content_settings::ProviderType::kPrefProvider);
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(managed_provider),
        content_settings::ProviderType::kPolicyProvider);
    auto service_wrapper = TestPrivacySandboxService(privacy_sandbox_service());

    privacy_sandbox_test_util::RunTestCase(
        browser_task_environment(), prefs(), host_content_settings_map(),
        mock_delegate(), mock_browsing_topics_service(),
        privacy_sandbox_settings(), &service_wrapper, user_provider_raw,
        managed_provider_raw, TestCase(test_state, test_input, test_output));
  }

  TestingProfile* profile() { return &profile_; }
  PrivacySandboxServiceImpl* privacy_sandbox_service() {
    return privacy_sandbox_service_.get();
  }
  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return privacy_sandbox_settings_.get();
  }
  base::test::ScopedFeatureList* feature_list() { return &inner_feature_list_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(profile()).get();
  }
  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings() {
    return TrackingProtectionSettingsFactory::GetForProfile(profile());
  }
  TestInterestGroupManager* test_interest_group_manager() {
    return &test_interest_group_manager_;
  }
  content::BrowsingDataRemover* browsing_data_remover() {
    return profile()->GetBrowsingDataRemover();
  }
  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return &mock_browsing_topics_service_;
  }
  privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate*
  mock_delegate() {
    return mock_delegate_;
  }
  first_party_sets::ScopedMockFirstPartySetsHandler&
  mock_first_party_sets_handler() {
    return mock_first_party_sets_handler_;
  }
  first_party_sets::FirstPartySetsPolicyService*
  first_party_sets_policy_service() {
    return &first_party_sets_policy_service_;
  }

  MockPrivacySandboxCountries* mock_privacy_sandbox_countries() {
    return mock_privacy_sandbox_countries_.get();
  }

  content::BrowserTaskEnvironment* browser_task_environment() {
    return &browser_task_environment_;
  }
#if !BUILDFLAG(IS_ANDROID)
  MockTrustSafetySentimentService* mock_sentiment_service() {
    return mock_sentiment_service_.get();
  }
#endif

 protected:
  base::HistogramTester histogram_tester;
  std::unique_ptr<privacy_sandbox::PrivacySandboxNoticeStorage> notice_storage_;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;

  TestingProfile profile_;
  base::test::ScopedFeatureList outer_feature_list_;
  base::test::ScopedFeatureList inner_feature_list_;
  TestInterestGroupManager test_interest_group_manager_;
  browsing_topics::MockBrowsingTopicsService mock_browsing_topics_service_;

  first_party_sets::ScopedMockFirstPartySetsHandler
      mock_first_party_sets_handler_;
  first_party_sets::FirstPartySetsPolicyService
      first_party_sets_policy_service_ =
          first_party_sets::FirstPartySetsPolicyService(
              profile_.GetOriginalProfile());
  std::unique_ptr<MockPrivacySandboxCountries> mock_privacy_sandbox_countries_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MockTrustSafetySentimentService> mock_sentiment_service_;
#endif
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
      mock_delegate_;  // Owned by |privacy_sandbox_settings_|.
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations_;

  std::unique_ptr<PrivacySandboxServiceImpl> privacy_sandbox_service_;
};

// Params correspond to (IsFeatureOn, IsConsentCountry, ExpectedResult).
class PrivacySandboxPrivacyGuideShouldShowAdTopicsTest
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {};

TEST_P(PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
       ShownAccordingToConsentCountryAndFeature) {
  bool is_feature_on = static_cast<bool>(std::get<0>(GetParam()));
  bool is_consent_country = static_cast<bool>(std::get<1>(GetParam()));
  bool result = static_cast<bool>(std::get<2>(GetParam()));

  feature_list()->Reset();
  if (is_feature_on) {
    feature_list()->InitAndEnableFeature(
        privacy_sandbox::kPrivacySandboxPrivacyGuideAdTopics);
  }

  ON_CALL(*mock_privacy_sandbox_countries(), IsConsentCountry())
      .WillByDefault(testing::Return(is_consent_country));

  bool should_show_card =
      privacy_sandbox_service()
          ->PrivacySandboxPrivacyGuideShouldShowAdTopicsCard();
  ASSERT_EQ(should_show_card, result);
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
                         PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
                         testing::Values(std::tuple(true, true, true),
                                         std::tuple(true, false, false),
                                         std::tuple(false, true, false),
                                         std::tuple(false, false, false)));

TEST_F(PrivacySandboxServiceTest, GetFledgeJoiningEtldPlusOne) {
  // Confirm that the set of FLEDGE origins which were top-frame for FLEDGE join
  // actions is correctly converted into a list of eTLD+1s.

  using FledgeTestCase =
      std::pair<std::vector<url::Origin>, std::vector<std::string>>;

  // Items which map to the same eTLD+1 should be coalesced into a single entry.
  FledgeTestCase test_case_1 = {
      {url::Origin::Create(GURL("https://www.example.com")),
       url::Origin::Create(GURL("https://example.com:8080")),
       url::Origin::Create(GURL("http://www.example.com"))},
      {"example.com"}};

  // eTLD's should return the host instead, this is relevant for sites which
  // are themselves on the PSL, e.g. github.io.
  FledgeTestCase test_case_2 = {
      {
          url::Origin::Create(GURL("https://co.uk")),
          url::Origin::Create(GURL("http://co.uk")),
          url::Origin::Create(GURL("http://example.co.uk")),
      },
      {"co.uk", "example.co.uk"}};

  // IP addresses should also return the host.
  FledgeTestCase test_case_3 = {
      {
          url::Origin::Create(GURL("https://192.168.1.2")),
          url::Origin::Create(GURL("https://192.168.1.2:8080")),
          url::Origin::Create(GURL("https://192.168.1.3:8080")),
      },
      {"192.168.1.2", "192.168.1.3"}};

  // Results should be alphabetically ordered.
  FledgeTestCase test_case_4 = {{
                                    url::Origin::Create(GURL("https://d.com")),
                                    url::Origin::Create(GURL("https://b.com")),
                                    url::Origin::Create(GURL("https://a.com")),
                                    url::Origin::Create(GURL("https://c.com")),
                                },
                                {"a.com", "b.com", "c.com", "d.com"}};

  std::vector<FledgeTestCase> test_cases = {test_case_1, test_case_2,
                                            test_case_3, test_case_4};

  for (const auto& [origins, expected] : test_cases) {
    base::HistogramTester histogram_tester;
    test_interest_group_manager()->SetInterestGroupDataKeys(
        base::ToVector(origins, [](const auto& origin) {
          return content::InterestGroupManager::InterestGroupDataKey{
              url::Origin::Create(GURL("https://embedded.com")), origin};
        }));

    bool callback_called = false;
    auto callback = base::BindLambdaForTesting(
        [&](std::vector<std::string> items_for_display) {
          ASSERT_EQ(items_for_display.size(), expected.size());
          for (size_t i = 0; i < items_for_display.size(); i++) {
            EXPECT_EQ(expected[i], items_for_display[i]);
          }
          callback_called = true;
        });

    privacy_sandbox_service()->GetFledgeJoiningEtldPlusOneForDisplay(callback);
    EXPECT_TRUE(callback_called);
    histogram_tester.ExpectUniqueSample(
        "PrivacySandbox.ProtectedAudience.JoiningTopFrameDisplayed", true,
        origins.size());
  }
}

TEST_F(PrivacySandboxServiceTest, GetFledgeJoiningEtldPlusOne_InvalidTopFrame) {
  // Confirm that when an invalid top frame is received, the appropriate metric
  // is recorded, and the returned list is empty.
  base::HistogramTester histogram_tester;
  auto missing_top_frame = content::InterestGroupManager::InterestGroupDataKey{
      url::Origin::Create(GURL("https://embedded.com")), url::Origin()};
  test_interest_group_manager()->SetInterestGroupDataKeys({missing_top_frame});

  bool callback_called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::vector<std::string> items_for_display) {
        ASSERT_EQ(items_for_display.size(), 0u);
        callback_called = true;
      });

  privacy_sandbox_service()->GetFledgeJoiningEtldPlusOneForDisplay(callback);
  EXPECT_TRUE(callback_called);
  histogram_tester.ExpectUniqueSample(
      "PrivacySandbox.ProtectedAudience.JoiningTopFrameDisplayed", false, 1);
}

TEST_F(PrivacySandboxServiceTest, GetFledgeBlockedEtldPlusOne) {
  // Confirm that blocked FLEDGE top frame eTLD+1's are correctly produced
  // for display.
  const std::vector<std::string> sites = {"google.com", "example.com",
                                          "google.com.au"};
  for (const auto& site : sites) {
    privacy_sandbox_settings()->SetFledgeJoiningAllowed(site, false);
  }

  // Sites should be returned in lexographical order.
  auto returned_sites =
      privacy_sandbox_service()->GetBlockedFledgeJoiningTopFramesForDisplay();
  ASSERT_EQ(returned_sites.size(), 3u);
  EXPECT_EQ(returned_sites[0], sites[1]);
  EXPECT_EQ(returned_sites[1], sites[0]);
  EXPECT_EQ(returned_sites[2], sites[2]);

  // Settings a site back to allowed should appropriately remove it from the
  // display list.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("google.com", true);
  returned_sites =
      privacy_sandbox_service()->GetBlockedFledgeJoiningTopFramesForDisplay();
  ASSERT_EQ(returned_sites.size(), 2u);
  EXPECT_EQ(returned_sites[0], sites[1]);
  EXPECT_EQ(returned_sites[1], sites[2]);
}

TEST_F(PrivacySandboxServiceTest, HistogramsAreEmptyOnStartup) {
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  for (const auto& notice_name : privacy_sandbox::kPrivacySandboxNoticeNames) {
    EXPECT_THAT(
        histograms,
        testing::Not(testing::AnyOf(base::StrCat(
            {"PrivacySandbox.Notice.NoticeStartupState.", notice_name}))));
  }
}

TEST_F(PrivacySandboxServiceTest, PromptActionsUMAActions) {
  base::UserActionTester user_action_tester;

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"}});
  privacy_sandbox_service()->PromptActionOccurred(PromptAction::kNoticeShown,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(
      user_action_tester.GetActionCount("Settings.PrivacySandbox.Notice.Shown"),
      1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kNoticeOpenSettings, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.OpenedSettings"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kNoticeAcknowledge, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.Acknowledged"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(PromptAction::kNoticeDismiss,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.Dismissed"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kNoticeClosedNoInteraction, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.ClosedNoInteraction"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kNoticeLearnMore, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.LearnMore"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kNoticeMoreInfoOpened, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.LearnMoreExpanded"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kNoticeMoreInfoClosed, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.LearnMoreClosed"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentMoreButtonClicked, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.MoreButtonClicked"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kNoticeMoreButtonClicked, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.MoreButtonClicked"),
            1);

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName, "true"}});

  privacy_sandbox_service()->PromptActionOccurred(PromptAction::kConsentShown,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.Shown"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentAccepted, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.Accepted"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentDeclined, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.Declined"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentMoreInfoOpened, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.LearnMoreExpanded"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kPrivacyPolicyLinkClicked, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.PrivacyPolicyLinkClicked"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentMoreInfoClosed, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.LearnMoreClosed"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentClosedNoDecision, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.ClosedNoInteraction"),
            1);

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName, "true"},
       {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
        "true"}});

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kRestrictedNoticeOpenSettings, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.OpenedSettings"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kRestrictedNoticeAcknowledge, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.Acknowledged"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kRestrictedNoticeShown, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.Shown"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kRestrictedNoticeClosedNoInteraction,
      SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.ClosedNoInteraction"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kRestrictedNoticeMoreButtonClicked, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.MoreButtonClicked"),
            1);
}

TEST_F(PrivacySandboxServiceTest, FledgeBlockDeletesData) {
  // Allowing FLEDGE joining should not start a removal task.
  privacy_sandbox_service()->SetFledgeJoiningAllowed("example.com", true);
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            0xffffffffffffffffull);  // -1, indicates no last removal task.

  // When FLEDGE joining is blocked, a removal task should be started.
  privacy_sandbox_service()->SetFledgeJoiningAllowed("example.com", false);
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS);
  EXPECT_EQ(browsing_data_remover()->GetLastUsedBeginTimeForTesting(),
            base::Time::Min());
  EXPECT_EQ(browsing_data_remover()->GetLastUsedOriginTypeMaskForTesting(),
            content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
}

TEST_F(PrivacySandboxServiceTest, DisablingTopicsPrefClearsData) {
  // Confirm that when the topics preference is disabled, topics data is
  // deleted. No browsing data remover tasks are started.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  // Enabling should not delete data.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  constexpr uint64_t kNoRemovalTask = -1ull;
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            kNoRemovalTask);

  // Disabling should start delete topics data.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(1);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            kNoRemovalTask);
}

TEST_F(PrivacySandboxServiceTest, DisablingFledgePrefClearsData) {
  // Confirm that when the fledge preference is disabled, a browsing data
  // remover task is started. Topics data isn't deleted.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  // Enabling should not cause a removal task.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  constexpr uint64_t kNoRemovalTask = -1ull;
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            kNoRemovalTask);

  // Disabling should start a task clearing all related information.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
  EXPECT_EQ(
      browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
      content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS |
          content::BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE |
          content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS_INTERNAL);
  EXPECT_EQ(browsing_data_remover()->GetLastUsedBeginTimeForTesting(),
            base::Time::Min());
  EXPECT_EQ(browsing_data_remover()->GetLastUsedOriginTypeMaskForTesting(),
            content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
}

TEST_F(PrivacySandboxServiceTest, DisablingAdMeasurementePrefClearsData) {
  // Confirm that when the ad measurement preference is disabled, a browsing
  // data remover task is started. Topics data isn't deleted.
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  // Enabling should not cause a removal task.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  constexpr uint64_t kNoRemovalTask = -1ull;
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            kNoRemovalTask);

  // Disabling should start a task clearing all related information.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, false);
  EXPECT_EQ(
      browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
      content::BrowsingDataRemover::DATA_TYPE_ATTRIBUTION_REPORTING |
          content::BrowsingDataRemover::DATA_TYPE_AGGREGATION_SERVICE |
          content::BrowsingDataRemover::DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL);
  EXPECT_EQ(browsing_data_remover()->GetLastUsedBeginTimeForTesting(),
            base::Time::Min());
  EXPECT_EQ(browsing_data_remover()->GetLastUsedOriginTypeMaskForTesting(),
            content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
}

TEST_F(PrivacySandboxServiceTest, GetTopTopics) {
  // Check that the service correctly de-dupes and orders top topics. Topics
  // should be alphabetically ordered.
  const privacy_sandbox::CanonicalTopic kFirstTopic =
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(24),  // "Blues"
                                      kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kSecondTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(23),  // "Music & audio"
          kTestTaxonomyVersion);

  const std::vector<privacy_sandbox::CanonicalTopic> kTopTopics = {
      kSecondTopic, kSecondTopic, kFirstTopic};

  EXPECT_CALL(*mock_browsing_topics_service(), GetTopTopicsForDisplay())
      .WillOnce(testing::Return(kTopTopics));

  auto topics = privacy_sandbox_service()->GetCurrentTopTopics();

  ASSERT_EQ(topics.size(), 2u);
  EXPECT_EQ(topics[0], kFirstTopic);
  EXPECT_EQ(topics[1], kSecondTopic);
}

TEST_F(PrivacySandboxServiceTest, GetBlockedTopics) {
  // Check that blocked topics are correctly alphabetically sorted and returned.
  const privacy_sandbox::CanonicalTopic kFirstTopic =
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(24),  // "Blues"
                                      kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kSecondTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(23),  // "Music & audio"
          kTestTaxonomyVersion);

  // The PrivacySandboxService assumes that the PrivacySandboxSettings service
  // dedupes blocked topics. Check that assumption here.
  privacy_sandbox_settings()->SetTopicAllowed(kSecondTopic, false);
  privacy_sandbox_settings()->SetTopicAllowed(kSecondTopic, false);
  privacy_sandbox_settings()->SetTopicAllowed(kFirstTopic, false);
  privacy_sandbox_settings()->SetTopicAllowed(kFirstTopic, false);

  auto blocked_topics = privacy_sandbox_service()->GetBlockedTopics();

  ASSERT_EQ(blocked_topics.size(), 2u);
  EXPECT_EQ(blocked_topics[0], kFirstTopic);
  EXPECT_EQ(blocked_topics[1], kSecondTopic);
}

TEST_F(PrivacySandboxServiceTest, GetFirstLevelTopics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kBrowsingTopicsParameters, {{"taxonomy_version", "2"}});

  // Check that blocked topics are correctly alphabetically sorted and returned.
  const privacy_sandbox::CanonicalTopic kFirstTopic =
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(1),
                                      kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kLastTopic =
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(332),
                                      kTestTaxonomyVersion);

  auto first_level_topics = privacy_sandbox_service()->GetFirstLevelTopics();

  ASSERT_EQ(first_level_topics.size(), 22u);
  EXPECT_EQ(first_level_topics[0], kFirstTopic);
  EXPECT_EQ(first_level_topics[21], kLastTopic);
}

TEST_F(PrivacySandboxServiceTest, GetChildTopicsCurrentlyAssigned) {
  const privacy_sandbox::CanonicalTopic kParentTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(1),  // "Arts & Entertainment"
          kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kDirectChildTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(23),  // "Music & audio"
          kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kIndirectChildTopic =
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(29),  // "Jazz"
                                      kTestTaxonomyVersion);
  const privacy_sandbox::CanonicalTopic kNotChildTopic =
      privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(99),  // "Hair Care"
          kTestTaxonomyVersion);

  // No child topic assigned initially.
  auto currently_assigned_child_topics =
      privacy_sandbox_service()->GetChildTopicsCurrentlyAssigned(kParentTopic);
  ASSERT_EQ(0u, currently_assigned_child_topics.size());

  // Assign some topics.
  const std::vector<privacy_sandbox::CanonicalTopic> kTopTopics = {
      kDirectChildTopic, kIndirectChildTopic, kNotChildTopic};
  ON_CALL(*mock_browsing_topics_service(), GetTopTopicsForDisplay())
      .WillByDefault(testing::Return(kTopTopics));

  // Both direct and indirect child should be returned.
  currently_assigned_child_topics =
      privacy_sandbox_service()->GetChildTopicsCurrentlyAssigned(kParentTopic);
  ASSERT_EQ(currently_assigned_child_topics.size(), 2u);
  EXPECT_EQ(currently_assigned_child_topics[0], kIndirectChildTopic);
  EXPECT_EQ(currently_assigned_child_topics[1], kDirectChildTopic);
}

TEST_F(PrivacySandboxServiceTest, SetTopicAllowed) {
  const privacy_sandbox::CanonicalTopic kTestTopic =
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(10),
                                      kTestTaxonomyVersion);
  EXPECT_CALL(*mock_browsing_topics_service(), ClearTopic(kTestTopic)).Times(1);
  privacy_sandbox_service()->SetTopicAllowed(kTestTopic, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(kTestTopic));

  testing::Mock::VerifyAndClearExpectations(mock_browsing_topics_service());
  EXPECT_CALL(*mock_browsing_topics_service(), ClearTopic(kTestTopic)).Times(0);
  privacy_sandbox_service()->SetTopicAllowed(kTestTopic, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(kTestTopic));
}

TEST_F(PrivacySandboxServiceTest, TestNoFakeTopics) {
  auto* service = privacy_sandbox_service();
  EXPECT_THAT(service->GetCurrentTopTopics(), testing::IsEmpty());
  EXPECT_THAT(service->GetBlockedTopics(), testing::IsEmpty());
}

TEST_F(PrivacySandboxServiceTest, TestNoFakeTopicsPrefOff) {
  // Sample data won't be returned for current topics when the pref is off, only
  // the blocked list.
  prefs()->SetUserPref(prefs::kPrivacySandboxM1TopicsEnabled,
                       std::make_unique<base::Value>(false));

  feature_list()->InitWithFeaturesAndParameters(
      {{privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting
              .name,
          "true"}}}},
      {});

  CanonicalTopic topic3(Topic(3), kTestTaxonomyVersion);
  CanonicalTopic topic4(Topic(4), kTestTaxonomyVersion);

  auto* service = privacy_sandbox_service();
  EXPECT_THAT(service->GetCurrentTopTopics(), testing::IsEmpty());
  EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic3, topic4));
}

TEST_F(PrivacySandboxServiceTest, TestFakeTopics) {
  std::vector<base::test::FeatureRefAndParams> test_features = {
      {privacy_sandbox::kPrivacySandboxSettings4,
       {{privacy_sandbox::kPrivacySandboxSettings4ShowSampleDataForTesting.name,
         "true"}}}};

  // Sample data for current topics is only returned when the pref is on.
  prefs()->SetUserPref(prefs::kPrivacySandboxM1TopicsEnabled,
                       std::make_unique<base::Value>(true));

  for (const auto& feature : test_features) {
    feature_list()->Reset();
    feature_list()->InitWithFeaturesAndParameters({feature}, {});
    CanonicalTopic topic1(Topic(1), kTestTaxonomyVersion);
    CanonicalTopic topic2(Topic(2), kTestTaxonomyVersion);
    CanonicalTopic topic3(Topic(3), kTestTaxonomyVersion);
    CanonicalTopic topic4(Topic(4), kTestTaxonomyVersion);
    // Duplicate a topic to test that it doesn't appear in the results in
    // addition to topic4.
    CanonicalTopic topic4_duplicate(Topic(4), kTestTaxonomyVersion - 1);

    auto* service = privacy_sandbox_service();
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic1, topic2));
    EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic3, topic4));

    service->SetTopicAllowed(topic1, false);
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic2));
    EXPECT_THAT(service->GetBlockedTopics(),
                ElementsAre(topic1, topic3, topic4));

    service->SetTopicAllowed(topic4, true);
    service->SetTopicAllowed(topic4_duplicate, true);
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic2, topic4));
    EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic1, topic3));

    service->SetTopicAllowed(topic1, true);
    service->SetTopicAllowed(topic4, false);
    service->SetTopicAllowed(topic4_duplicate, false);
    EXPECT_THAT(service->GetCurrentTopTopics(), ElementsAre(topic1, topic2));
    EXPECT_THAT(service->GetBlockedTopics(), ElementsAre(topic3, topic4));
  }
}
using PrivacySandboxServiceDeathTest = PrivacySandboxServiceTest;

TEST_F(PrivacySandboxServiceDeathTest, TPSettingsNullExpectDeath) {
  ASSERT_DEATH(
      {
        PrivacySandboxServiceImpl(
            privacy_sandbox_settings(),
            /*tracking_protection_settings=*/nullptr, cookie_settings(),
            profile()->GetPrefs(), test_interest_group_manager(),
            GetProfileType(), browsing_data_remover(),
            host_content_settings_map(),
#if !BUILDFLAG(IS_ANDROID)
            mock_sentiment_service(),
#endif
            mock_browsing_topics_service(), first_party_sets_policy_service(),
            mock_privacy_sandbox_countries());
      },
      "");
}

TEST_F(PrivacySandboxServiceTest,
       FirstPartySetsNotRelevantMetricAllowedCookies) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsNotRelevant, 1);
}

TEST_F(PrivacySandboxServiceTest,
       FirstPartySetsNotRelevantMetricBlockedCookies) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsNotRelevant, 1);
}

TEST_F(PrivacySandboxServiceTest, FirstPartySetsEnabledMetric) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, FirstPartySetsDisabledMetric) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsDisabled, 1);
}

TEST_F(PrivacySandboxServiceTest, SampleFpsData) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
      {{"use-sample-sets", "true"}});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://mail.google.com.au")),
            u"google.com");
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://youtube.com")),
            u"google.com");
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://muenchen.de")),
            u"münchen.de");
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwnerForDisplay(
                GURL("https://example.com")),
            std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledWhen3pcAllowed) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary,
                                    std::nullopt)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                    0)}},
      },
      {});

  // Simulate 3PC are allowed while:
  // - FPS pref is enabled
  // - FPS UI Feature is enabled
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI}, {});
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledWhenAllCookiesBlocked) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary,
                                    std::nullopt)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                    0)}},
      },
      {});

  // Simulate all cookies are blocked while:
  // - FPS pref is enabled
  // - FPS UI Feature is enabled
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI}, {});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledByFpsUiFeature) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary,
                                    std::nullopt)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                    0)}},
      },
      {});

  // Simulate FPS UI feature disabled while:
  // - FPS pref is enabled
  // - 3PC are being blocked
  feature_list()->InitWithFeatures(
      {}, {privacy_sandbox::kPrivacySandboxFirstPartySetsUI});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();

  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetFirstPartySetOwner_SimulatedFpsData_DisabledByFpsPref) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kFirstPartySetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary,
                                    std::nullopt)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                    0)}},
      },
      {});

  // Simulate FPS pref disabled while:
  // - FPS UI Feature is enabled
  // - 3PC are being blocked
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI}, {});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(false));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();

  // We shouldn't get associate1's owner since FPS is disabled.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       SimulatedFpsData_FpsEnabled_WithoutGlobalSets) {
  GURL primary_gurl("https://primary.test");
  GURL associate1_gurl("https://associate1.test");
  GURL associate2_gurl("https://associate2.test");
  net::SchemefulSite primary_site(primary_gurl);
  net::SchemefulSite associate1_site(associate1_gurl);
  net::SchemefulSite associate2_site(associate2_gurl);

  // Set up state that fully enables the First-Party Sets for UI; blocking 3PC,
  // and enabling the FPS UI feature and the FPS enabled pref.
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI}, {});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Verify `GetFirstPartySetOwner` returns empty if FPS is enabled but the
  // Global sets are not ready yet.
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl),
            std::nullopt);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate2_gurl),
            std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       SimulatedFpsData_FpsEnabled_WithGlobalSetsAndProfileSets) {
  GURL primary_gurl("https://primary.test");
  GURL associate1_gurl("https://associate1.test");
  GURL associate2_gurl("https://associate2.test");
  net::SchemefulSite primary_site(primary_gurl);
  net::SchemefulSite associate1_site(associate1_gurl);
  net::SchemefulSite associate2_site(associate2_gurl);

  // Set up state that fully enables the First-Party Sets for UI; blocking 3PC,
  // and enabling the FPS UI feature and the FPS enabled pref.
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI}, {});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Simulate that the Global First-Party Sets are ready with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test", "https://associate2.test"] }
  mock_first_party_sets_handler().SetGlobalSets(net::GlobalFirstPartySets(
      kFirstPartySetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary,
                                    std::nullopt)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                    0)}},
          {associate2_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                    1)}},
      },
      {}));

  // Simulate that associate2 is removed from the Global First-Party Sets for
  // this profile.
  mock_first_party_sets_handler().SetContextConfig(
      net::FirstPartySetsContextConfig(
          {{net::SchemefulSite(GURL("https://associate2.test")),
            net::FirstPartySetEntryOverride()}}));

  first_party_sets_policy_service()->InitForTesting();

  // Verify that primary owns associate1, but no longer owns associate2.
  EXPECT_EQ(
      privacy_sandbox_service()->GetFirstPartySetOwner(associate1_gurl).value(),
      primary_site);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(associate2_gurl),
            std::nullopt);
}

TEST_F(PrivacySandboxServiceTest, FpsPrefInit) {
  // Check that the init of the FPS pref occurs correctly.
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));

  // Whilst the FPS UI is not available, the pref should not be init.
  feature_list()->InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxFirstPartySetsUI);

  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  // If the UI is available, the user blocks 3PC, and the pref has not been
  // previously init, it should be.
  ClearFpsUserPrefs(prefs());
  feature_list()->Reset();
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxFirstPartySetsUI);

  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  // Once the pref has been init, it should not be re-init, and updated user
  // cookie settings should not impact it.
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));

  // Blocking all cookies should also init the FPS pref to off.
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized));
}

TEST_F(PrivacySandboxServiceTest, UsesFpsSampleSetsWhenProvided) {
  // Confirm that when the FPS sample sets are provided, they are used to answer
  // First-Party Sets queries instead of the actual sets.

  // Set up state that fully enables the First-Party Sets for UI; blocking
  // 3PC, and enabling the FPS UI feature and the FPS enabled pref.
  //
  // Note: this indicates that the sample sets should be used.
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{{privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
                             {{"use-sample-sets", "true"}}}},
      /*disabled_features=*/{});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Simulate that the Global First-Party Sets are ready with the following
  // set:
  // { primary: "https://youtube-primary.test",
  // associatedSites: ["https://youtube.com"]
  // }
  net::SchemefulSite youtube_primary_site(GURL("https://youtube-primary.test"));
  GURL youtube_gurl("https://youtube.com");
  net::SchemefulSite youtube_site(youtube_gurl);

  mock_first_party_sets_handler().SetGlobalSets(net::GlobalFirstPartySets(
      kFirstPartySetsVersion,
      {
          {youtube_primary_site,
           {net::FirstPartySetEntry(youtube_primary_site,
                                    net::SiteType::kPrimary, std::nullopt)}},
          {youtube_site,
           {net::FirstPartySetEntry(youtube_primary_site,
                                    net::SiteType::kAssociated, 0)}},
      },
      {}));

  // Simulate that https://google.de is moved into a new First-Party Set for
  // this profile.
  mock_first_party_sets_handler().SetContextConfig(
      net::FirstPartySetsContextConfig(
          {{net::SchemefulSite(GURL("https://google.de")),
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                net::SchemefulSite(GURL("https://new-primary.test")),
                net::SiteType::kAssociated, 0))}}));

  first_party_sets_policy_service()->InitForTesting();

  // Expect queries to be resolved based on the FPS sample sets.
  EXPECT_GT(privacy_sandbox_service()->GetSampleFirstPartySets().size(), 0u);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(
                GURL("https://youtube.com")),
            net::SchemefulSite(GURL("https://google.com")));
  EXPECT_TRUE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://googlesource.com"))));
  EXPECT_FALSE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://google.de"))));

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {privacy_sandbox::kPrivacySandboxFirstPartySetsUI}, {});
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearFpsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Expect queries to be resolved based on the FPS backend.
  EXPECT_EQ(privacy_sandbox_service()->GetSampleFirstPartySets().size(), 0u);
  EXPECT_EQ(privacy_sandbox_service()->GetFirstPartySetOwner(youtube_gurl),
            youtube_primary_site);
  EXPECT_FALSE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://googlesource.com"))));
  EXPECT_TRUE(privacy_sandbox_service()->IsPartOfManagedFirstPartySet(
      net::SchemefulSite(GURL("https://google.de"))));
}

TEST_F(PrivacySandboxServiceTest, TopicsConsentDefault) {
  RunTestCase(
      TestState{}, TestInput{},
      TestOutput{{kTopicsConsentGiven, false},
                 {kTopicsConsentLastUpdateReason,
                  privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue},
                 {kTopicsConsentLastUpdateTime, base::Time()},
                 {kTopicsConsentStringIdentifiers, std::vector<int>()}});
}

TEST_F(PrivacySandboxServiceTest, TopicsConsentSettings_EnableWithBlocked) {
  // Note that when testing for enabling topics, there can never have been
  // current topics in prod code.
  RunTestCase(
      TestState{{kActiveTopicsConsent, false},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, true},
      },
      TestOutput{
          {kTopicsConsentGiven, true},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/true,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/true)},
      });
}

TEST_F(PrivacySandboxServiceTest, TopicsConsentSettings_EnableNoBlocked) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, false},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, true},
      },
      TestOutput{
          {kTopicsConsentGiven, true},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/true,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/false)},
      });
}

TEST_F(PrivacySandboxServiceTest,
       TopicsConsentSettings_DisableCurrentAndBlocked) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, true},
                {kHasBlockedTopics, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/true,
                                              /*has_blocked_topics=*/true)},
      });
}

TEST_F(PrivacySandboxServiceTest, TopicsConsentSettings_DisableBlockedOnly) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/true)},
      });
}

TEST_F(PrivacySandboxServiceTest, TopicsConsentSettings_DisableCurrentOnly) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, true},
                {kHasBlockedTopics, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/true,
                                              /*has_blocked_topics=*/false)},
      });
}

TEST_F(PrivacySandboxServiceTest,
       TopicsConsentSettings_DisableNoCurrentNoBlocked) {
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kHasCurrentTopics, false},
                {kHasBlockedTopics, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kTopicsToggleNewValue, false},
      },
      TestOutput{
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kSettings},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsSettingsStringIdentifiers(/*did_consent=*/false,
                                              /*has_current_topics=*/false,
                                              /*has_blocked_topics=*/false)},
      });
}

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Explicitly) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kRestricted));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueToPrivacySandboxRestricted),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PromptSuppressedReason::kThirdPartyCookiesBlocked));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueTo3PCBlocked),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PromptSuppressedReason::kTrialsConsentDeclined));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueToTrialConsentDeclined),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PromptSuppressedReason::kTrialsDisabledAfterNotice));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueToTrialsDisabledAfterNoticeShown),
      /*expected_count=*/1);

  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kPolicy));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueToManagedState),
      /*expected_count=*/1);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kRestrictedNoticeNotShownDueToNoticeShownToGuardian),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Implicitly) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kNone));

  // Disable one of the K-APIs.
  prefs()->SetManagedPref(prefs::kPrivacySandboxM1TopicsEnabled,
                          base::Value(false));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueToManagedState),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_EEA) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kNone));

  base::test::ScopedFeatureList feature_list_consent_required;
  std::map<std::string, std::string> consent_required_feature_param = {
      {std::string(
           privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName),
       "true"},
      {std::string(privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName),
       "false"}};
  feature_list_consent_required.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      consent_required_feature_param);
  // Not consented
  prefs()->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade, false);

  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kEEAConsentPromptWaiting),
      /*expected_count=*/1);

  // Consent decision made and notice acknowledged.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1ConsentDecisionMade, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged, true);

  // With topics enabled.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kEEAFlowCompletedWithTopicsAccepted),
      /*expected_count=*/1);

  // With topics disabled.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kEEAFlowCompletedWithTopicsDeclined),
      /*expected_count=*/1);

  // Consent decision made but notice was not acknowledged.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged, false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kEEANoticePromptWaiting),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_ROW) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kNone));

  base::test::ScopedFeatureList feature_list_notice_required;
  std::map<std::string, std::string> notice_required_feature_param = {
      {std::string(
           privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName),
       "false"},
      {std::string(privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName),
       "true"}};
  feature_list_notice_required.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4, notice_required_feature_param);

  // Notice flow not completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged, false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kROWNoticePromptWaiting),
      /*expected_count=*/1);

  // Notice flow completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kROWNoticeFlowCompleted),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceTest, RecordPrivacySandbox4StartupMetrics_APIs) {
  // Each test for the APIs are scoped below to ensure we start with a clean
  // HistogramTester as each call to `RecordPrivacySandbox4StartupMetrics` emits
  // histograms for all APIs.

  // Topics
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Topics.Enabled",
                                       static_cast<int>(true),
                                       /*expected_count=*/1);

    prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Topics.Enabled",
                                       static_cast<int>(false),
                                       /*expected_count=*/1);
  }

  // Fledge
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Fledge.Enabled",
                                       static_cast<int>(true),
                                       /*expected_count=*/1);
    prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Fledge.Enabled",
                                       static_cast<int>(false),
                                       /*expected_count=*/1);
  }

  // Ad measurement
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.Enabled", static_cast<int>(true),
        /*expected_count=*/1);
    prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.Enabled",
        static_cast<int>(false),
        /*expected_count=*/1);
  }
}

class PrivacySandboxNoticeActionToStorageTests
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<NoticeTestingParameters> {};

using TopicsConsentTest = PrivacySandboxNoticeActionToStorageTests;
using NoticeAckTest = PrivacySandboxNoticeActionToStorageTests;
using NoticeShownTest = PrivacySandboxNoticeActionToStorageTests;
using NoticeSettingsTest = PrivacySandboxNoticeActionToStorageTests;

TEST_P(TopicsConsentTest, DidConsentOptInUpdateNoticeStorage) {
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{GetParam().feature_flag,
                            {privacy_sandbox::kPsDualWritePrefsToNoticeStorage,
                             {}}},
      /*disabled_features=*/{});

  // Show then OptIn
  privacy_sandbox_service()->PromptActionOccurred(PromptAction::kConsentShown,
                                                  GetParam().surface_type);
  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentAccepted, GetParam().surface_type);

  // Pref
  auto actual =
      notice_storage_->ReadNoticeData(prefs(), GetParam().notice_name);
  EXPECT_EQ(privacy_sandbox::NoticeActionTaken::kOptIn,
            actual->notice_action_taken);

  // Histogram
  CreateService();
  histogram_tester.ExpectBucketCount(
      base::StrCat({"PrivacySandbox.Notice.NoticeStartupState.",
                    GetParam().notice_name}),
      privacy_sandbox::NoticeStartupState::kFlowCompletedWithOptIn, 1);
}

TEST_P(TopicsConsentTest, DidConsentOptOutUpdateNoticeStorage) {
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{GetParam().feature_flag,
                            {privacy_sandbox::kPsDualWritePrefsToNoticeStorage,
                             {}}},
      /*disabled_features=*/{});

  // Show then OptOut
  privacy_sandbox_service()->PromptActionOccurred(PromptAction::kConsentShown,
                                                  GetParam().surface_type);
  privacy_sandbox_service()->PromptActionOccurred(
      PromptAction::kConsentDeclined, GetParam().surface_type);

  // Pref
  auto actual =
      notice_storage_->ReadNoticeData(prefs(), GetParam().notice_name);
  EXPECT_EQ(privacy_sandbox::NoticeActionTaken::kOptOut,
            actual->notice_action_taken);

  // Histogram
  CreateService();
  histogram_tester.ExpectBucketCount(
      base::StrCat({"PrivacySandbox.Notice.NoticeStartupState.",
                    GetParam().notice_name}),
      privacy_sandbox::NoticeStartupState::kFlowCompletedWithOptOut, 1);
}

TEST_P(NoticeShownTest, NoticeShownUpdateNoticeStorage) {
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{GetParam().feature_flag,
                            {privacy_sandbox::kPsDualWritePrefsToNoticeStorage,
                             {}}},
      /*disabled_features=*/{});

  privacy_sandbox_service()->PromptActionOccurred(GetParam().shown_type,
                                                  GetParam().surface_type);

  auto actual =
      notice_storage_->ReadNoticeData(prefs(), GetParam().notice_name);
  EXPECT_TRUE(actual.has_value());
}

TEST_P(NoticeAckTest, DidNoticeAckUpdateNoticeStorage) {
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{GetParam().feature_flag,
                            {privacy_sandbox::kPsDualWritePrefsToNoticeStorage,
                             {}}},
      /*disabled_features=*/{});

  // Show then ack
  privacy_sandbox_service()->PromptActionOccurred(GetParam().shown_type,
                                                  GetParam().surface_type);
  privacy_sandbox_service()->PromptActionOccurred(GetParam().prompt_action,
                                                  GetParam().surface_type);

  // Pref
  auto actual =
      notice_storage_->ReadNoticeData(prefs(), GetParam().notice_name);
  EXPECT_EQ(privacy_sandbox::NoticeActionTaken::kAck,
            actual->notice_action_taken);

  // Histogram
  CreateService();
  histogram_tester.ExpectBucketCount(
      base::StrCat({"PrivacySandbox.Notice.NoticeStartupState.",
                    GetParam().notice_name}),
      privacy_sandbox::NoticeStartupState::kFlowCompleted, 1);
}

TEST_P(NoticeSettingsTest, DidNoticeSettingsUpdateNoticeStorage) {
  feature_list()->Reset();
  feature_list()->InitWithFeaturesAndParameters(
      /*enabled_features=*/{GetParam().feature_flag,
                            {privacy_sandbox::kPsDualWritePrefsToNoticeStorage,
                             {}}},
      /*disabled_features=*/{});

  // Show then open settings
  privacy_sandbox_service()->PromptActionOccurred(GetParam().shown_type,
                                                  GetParam().surface_type);
  privacy_sandbox_service()->PromptActionOccurred(GetParam().prompt_action,
                                                  GetParam().surface_type);

  // Pref
  auto actual =
      notice_storage_->ReadNoticeData(prefs(), GetParam().notice_name);
  EXPECT_EQ(privacy_sandbox::NoticeActionTaken::kSettings,
            actual->notice_action_taken);

  // Histogram
  CreateService();
  histogram_tester.ExpectBucketCount(
      base::StrCat({"PrivacySandbox.Notice.NoticeStartupState.",
                    GetParam().notice_name}),
      privacy_sandbox::NoticeStartupState::kFlowCompleted, 1);
}

base::test::FeatureRefAndParams ConsentFeature() {
  return {
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName, "true"}}};
}

base::test::FeatureRefAndParams NoticeFeature() {
  return {
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"}}};
}

base::test::FeatureRefAndParams RestrictedNoticeFeature() {
  return {privacy_sandbox::kPrivacySandboxSettings4,
          {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
            "true"}}};
}

// The following tests test variations of all 4 notice storage prefs across 3
// surface types. For each promptAction that can be taken on the notices, we
// ensure the pref service and histograms were updated correctly.
INSTANTIATE_TEST_SUITE_P(
    NoticeShownTestSuite,
    NoticeShownTest,
    testing::ValuesIn<NoticeTestingParameters>(
        // Topics Consent Shown
        {{.surface_type = SurfaceType::kDesktop,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kConsentShown,
          .notice_name = privacy_sandbox::kTopicsConsentModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kConsentShown,
          .notice_name = privacy_sandbox::kTopicsConsentModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kConsentShown,
          .notice_name = privacy_sandbox::kTopicsConsentModalClankCCT},
         // EEA regional API notice Shown
         {.surface_type = SurfaceType::kDesktop,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .notice_name =
              privacy_sandbox::kProtectedAudienceMeasurementNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .notice_name = privacy_sandbox::
              kProtectedAudienceMeasurementNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .notice_name = privacy_sandbox::
              kProtectedAudienceMeasurementNoticeModalClankCCT},
         // ROW regional API notice Shown
         {.surface_type = SurfaceType::kDesktop,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModalClankCCT},
         // Restricted API notice Shown
         {.surface_type = SurfaceType::kDesktop,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .notice_name = privacy_sandbox::kMeasurementNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .notice_name = privacy_sandbox::kMeasurementNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .notice_name = privacy_sandbox::kMeasurementNoticeModalClankCCT}}));

INSTANTIATE_TEST_SUITE_P(
    TopicsConsentTestSuite,
    TopicsConsentTest,
    testing::ValuesIn<NoticeTestingParameters>(
        // Actions on consent (OptIn/OptOut)
        {{.surface_type = SurfaceType::kDesktop,
          .feature_flag = ConsentFeature(),
          .notice_name = privacy_sandbox::kTopicsConsentModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = ConsentFeature(),
          .notice_name = privacy_sandbox::kTopicsConsentModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = ConsentFeature(),
          .notice_name = privacy_sandbox::kTopicsConsentModalClankCCT}}));

INSTANTIATE_TEST_SUITE_P(
    NoticeTestSuite,
    NoticeAckTest,
    // Ack on ROW, EEA, Restricted Notices
    testing::ValuesIn<NoticeTestingParameters>(
        {{.surface_type = SurfaceType::kDesktop,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeAcknowledge,
          .notice_name =
              privacy_sandbox::kProtectedAudienceMeasurementNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeAcknowledge,
          .notice_name = privacy_sandbox::
              kProtectedAudienceMeasurementNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeAcknowledge,
          .notice_name = privacy_sandbox::
              kProtectedAudienceMeasurementNoticeModalClankCCT},
         {.surface_type = SurfaceType::kDesktop,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeAcknowledge,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeAcknowledge,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeAcknowledge,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModalClankCCT},
         {.surface_type = SurfaceType::kDesktop,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .prompt_action = PromptAction::kRestrictedNoticeAcknowledge,
          .notice_name = privacy_sandbox::kMeasurementNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .prompt_action = PromptAction::kRestrictedNoticeAcknowledge,
          .notice_name = privacy_sandbox::kMeasurementNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .prompt_action = PromptAction::kRestrictedNoticeAcknowledge,
          .notice_name = privacy_sandbox::kMeasurementNoticeModalClankCCT}}));

INSTANTIATE_TEST_SUITE_P(
    NoticeTestSuite,
    NoticeSettingsTest,
    testing::ValuesIn<NoticeTestingParameters>(
        // Settings click on ROW, EEA, Restricted Notices
        {{.surface_type = SurfaceType::kDesktop,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeOpenSettings,
          .notice_name =
              privacy_sandbox::kProtectedAudienceMeasurementNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeOpenSettings,
          .notice_name = privacy_sandbox::
              kProtectedAudienceMeasurementNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = ConsentFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeOpenSettings,
          .notice_name = privacy_sandbox::
              kProtectedAudienceMeasurementNoticeModalClankCCT},
         {.surface_type = SurfaceType::kDesktop,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeOpenSettings,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeOpenSettings,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = NoticeFeature(),
          .shown_type = PromptAction::kNoticeShown,
          .prompt_action = PromptAction::kNoticeOpenSettings,
          .notice_name = privacy_sandbox::kThreeAdsAPIsNoticeModalClankCCT},
         {.surface_type = SurfaceType::kDesktop,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .prompt_action = PromptAction::kRestrictedNoticeOpenSettings,
          .notice_name = privacy_sandbox::kMeasurementNoticeModal},
         {.surface_type = SurfaceType::kBrApp,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .prompt_action = PromptAction::kRestrictedNoticeOpenSettings,
          .notice_name = privacy_sandbox::kMeasurementNoticeModalClankBrApp},
         {.surface_type = SurfaceType::kAGACCT,
          .feature_flag = RestrictedNoticeFeature(),
          .shown_type = PromptAction::kRestrictedNoticeShown,
          .prompt_action = PromptAction::kRestrictedNoticeOpenSettings,
          .notice_name = privacy_sandbox::kMeasurementNoticeModalClankCCT}}));

class PrivacySandboxServiceM1RestrictedNoticeTest
    : public PrivacySandboxServiceTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeTest,
       RestrictedPromptActionsUpdatePrefs) {
  // Prompt acknowledge action should update the prefs accordingly.
  RunTestCase(
      TestState{{kM1AdMeasurementEnabledUserPrefValue, false},
                {kM1RestrictedNoticePreviouslyAcknowledged, false}},
      TestInput{{kPromptAction,
                 static_cast<int>(PromptAction::kRestrictedNoticeAcknowledge)}},
      TestOutput{{kM1AdMeasurementEnabled, true},
                 {kM1RestrictedNoticeAcknowledged, true}});

  // Open settings action should update the prefs accordingly.
  RunTestCase(TestState{{kM1AdMeasurementEnabledUserPrefValue, false},
                        {kM1RestrictedNoticePreviouslyAcknowledged, false}},
              TestInput{{kPromptAction,
                         static_cast<int>(
                             PromptAction::kRestrictedNoticeOpenSettings)}},
              TestOutput{{kM1AdMeasurementEnabled, true},
                         {kM1RestrictedNoticeAcknowledged, true}});
}

class PrivacySandboxServiceM1DelayCreation : public PrivacySandboxServiceTest {
 public:
  void SetUp() override {
    // Prevent service from being created by base class.
  }
};

TEST_F(PrivacySandboxServiceM1DelayCreation,
       UnrestrictedRemainsEnabledWithConsent) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, true);
  prefs()->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                   base::Time::Now());
  prefs()->SetInteger(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(
          privacy_sandbox::TopicsConsentUpdateSource::kConfirmation));
  prefs()->SetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate,
                     "foo");

  CreateService();

  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxTopicsConsentGiven));
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime),
            base::Time::Now());
  EXPECT_EQ(static_cast<privacy_sandbox::TopicsConsentUpdateSource>(
                prefs()->GetInteger(
                    prefs::kPrivacySandboxTopicsConsentLastUpdateReason)),
            privacy_sandbox::TopicsConsentUpdateSource::kConfirmation);
  EXPECT_EQ(
      prefs()->GetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate),
      "foo");
}

TEST_F(PrivacySandboxServiceM1DelayCreation,
       PromptSuppressReasonClearedWhenRestrictedNoticeEnabled) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
        "true"}});

  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kRestricted));

  CreateService();

  EXPECT_EQ(prefs()->GetValue(prefs::kPrivacySandboxM1PromptSuppressed),
            static_cast<int>(PromptSuppressedReason::kNone));
}

TEST_F(PrivacySandboxServiceM1DelayCreation,
       PromptSuppressReasonNotClearedWhenRestrictedNoticeDisabled) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
        "false"}});

  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kRestricted));

  CreateService();

  EXPECT_EQ(prefs()->GetValue(prefs::kPrivacySandboxM1PromptSuppressed),
            static_cast<int>(PromptSuppressedReason::kRestricted));
}

class PrivacySandboxServiceM1DelayCreationRestricted
    : public PrivacySandboxServiceM1DelayCreation {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/true);
    return mock_delegate;
  }
};

TEST_F(PrivacySandboxServiceM1DelayCreationRestricted,
       RestrictedDisablesAndClearsConsent) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, true);
  prefs()->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                   base::Time::Now());
  prefs()->SetInteger(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(
          privacy_sandbox::TopicsConsentUpdateSource::kConfirmation));
  prefs()->SetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate,
                     "foo");

  CreateService();

  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxTopicsConsentGiven));
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime),
            base::Time());
  EXPECT_EQ(static_cast<privacy_sandbox::TopicsConsentUpdateSource>(
                prefs()->GetInteger(
                    prefs::kPrivacySandboxTopicsConsentLastUpdateReason)),
            privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue);
  EXPECT_EQ(
      prefs()->GetString(prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate),
      "");
}

TEST_F(PrivacySandboxServiceM1DelayCreationRestricted,
       RestrictedEnabledDoesntClearAdMeasurementPref) {
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
        "true"}});

  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);

  CreateService();

  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
}

class PrivacySandboxServiceM1PromptTest : public PrivacySandboxServiceTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName, "true"},
         {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName,
          "false"}});
  }
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrivacySandboxServiceM1PromptTest, DeviceLocalAccountUser) {
  privacy_sandbox_service()->ForceChromeBuildForTests(true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::ScopedUserManager user_manager(
      std::make_unique<user_manager::FakeUserManager>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // No prompt should be shown for a public session account.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedTestPublicSessionLoginState login_state;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::BrowserInitParamsPtr init_params =
      crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  // TODO(crbug.com/361794340): Ensure the promptType is correct across
  // different surfaceTypes.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop),
      PromptType::kNone);

  // A prompt should be shown for a regular user.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kRegularSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  EXPECT_EQ(
      privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop),
      PromptType::kM1Consent);

  // No prompt should be shown for a web kiosk account.
  chromeos::SetUpFakeKioskSession();
  EXPECT_EQ(
      privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop),
      PromptType::kNone);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(PrivacySandboxServiceM1PromptTest, NonChromeBuildPrompt) {
  // A case that will normally show a prompt will not if is a non-Chrome build.
  RunTestCase(TestState{{kM1PromptPreviouslySuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)}},
              TestInput{{kForceChromeBuild, false}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}
#endif

TEST_F(PrivacySandboxServiceM1PromptTest, ThirdPartyCookiesBlockedPostTP3PC) {
  // If third party cookies are blocked, set the suppressed reason as
  // kThirdPartyCookiesBlocked and return kNone.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kBlockAll3pcToggleEnabledUserPrefValue, true},
                {kTrackingProtection3pcdEnabledUserPrefValue, true}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(
                      PromptSuppressedReason::kThirdPartyCookiesBlocked)}});
}

TEST_F(PrivacySandboxServiceM1PromptTest, ThirdPartyCookiesBlockedPreTP3PC) {
  // If third party cookies are blocked, set the suppressed reason as
  // kThirdPartyCookiesBlocked and return kNone.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kCookieControlsModeUserPrefValue,
                 content_settings::CookieControlsMode::kBlockThirdParty},
                {kTrackingProtection3pcdEnabledUserPrefValue, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(
                      PromptSuppressedReason::kThirdPartyCookiesBlocked)}});
}

TEST_F(PrivacySandboxServiceM1PromptTest, RestrictedPrompt) {
  // If the Privacy Sandbox is restricted, no prompt is shown.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kIsRestrictedAccount, true}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kRestricted)}});

  // After being restricted, even if the restriction is removed, no prompt
  // should be shown. No call should even need to be made to see if the
  // sandbox is still restricted.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kRestricted)},
                {kIsRestrictedAccount, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kRestricted)}});
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PrivacySandboxServiceM1PromptTest, PromptActionsSentimentService) {
  // Settings both consent and notice to be true so that we can loop through all
  // cases interacting with the sentiment service cleanly, without breaking
  // DCHECKs. Other tests / code paths check that PromptActionOccurred is
  // working correctly based on notice and consent, and assert that only one is
  // enabled.
  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName, "true"},
       {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"},
       {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
        "true"}});

  std::map<PromptAction, TrustSafetySentimentService::FeatureArea>
      expected_feature_areas;
  expected_feature_areas = {
      {PromptAction::kNoticeOpenSettings,
       TrustSafetySentimentService::FeatureArea::
           kPrivacySandbox4NoticeSettings},
      {PromptAction::kNoticeAcknowledge,
       TrustSafetySentimentService::FeatureArea::kPrivacySandbox4NoticeOk},
      {PromptAction::kConsentAccepted,
       TrustSafetySentimentService::FeatureArea::kPrivacySandbox4ConsentAccept},
      {PromptAction::kConsentDeclined,
       TrustSafetySentimentService::FeatureArea::
           kPrivacySandbox4ConsentDecline}};

  for (int enum_value = 0;
       enum_value <= static_cast<int>(PromptAction::kMaxValue); ++enum_value) {
    auto prompt_action = static_cast<PromptAction>(enum_value);
    if (expected_feature_areas.count(prompt_action)) {
      EXPECT_CALL(
          *mock_sentiment_service(),
          InteractedWithPrivacySandbox4(expected_feature_areas[prompt_action]))
          .Times(1);
    } else {
      EXPECT_CALL(*mock_sentiment_service(),
                  InteractedWithPrivacySandbox4(testing::_))
          .Times(0);
    }
    privacy_sandbox_service()->PromptActionOccurred(prompt_action,
                                                    SurfaceType::kDesktop);
    testing::Mock::VerifyAndClearExpectations(mock_sentiment_service());
  }
}
#endif

class PrivacySandboxServiceM1ConsentPromptTest
    : public PrivacySandboxServiceM1PromptTest {};

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, SuppressedConsent) {
  // A case that will normally show a consent will not if there is any
  // suppression reason.
  for (int suppressed_reason = static_cast<int>(PromptSuppressedReason::kNone);
       suppressed_reason <= static_cast<int>(PromptSuppressedReason::kMaxValue);
       ++suppressed_reason) {
    bool suppressed =
        suppressed_reason != static_cast<int>(PromptSuppressedReason::kNone);
    auto expected_prompt =
        suppressed ? PromptType::kNone : PromptType::kM1Consent;
    RunTestCase(
        TestState{{kM1PromptPreviouslySuppressedReason, suppressed_reason},
                  {kIsRestrictedAccount, false}},
        TestInput{{kForceChromeBuild, true}},
        TestOutput{{kPromptType, static_cast<int>(expected_prompt)},
                   {kM1PromptSuppressedReason, suppressed_reason}});
  }
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, TrialsConsentDeclined) {
  // If a previous consent decision was made to decline privacy sandbox, set
  // kTrialsConsentDeclined as suppressed reason and return kNone.
  // Now that the trials pref is deprecated users won't be able to enter that
  // state. Users who had the prompt suppressed due to declining the trials
  // consent should remain in this state.
  RunTestCase(
      TestState{
          {kM1PromptPreviouslySuppressedReason,
           static_cast<int>(PromptSuppressedReason::kTrialsConsentDeclined)},
          {kTrialsConsentDecisionMade, true}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{
          {kPromptType, static_cast<int>(PromptType::kNone)},
          {kM1PromptSuppressedReason,
           static_cast<int>(PromptSuppressedReason::kTrialsConsentDeclined)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, M1ConsentDecisionNotMade) {
  // If m1 consent required, and decision has not been made, return
  // kM1Consent.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kM1ConsentDecisionPreviouslyMade, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kM1Consent)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       M1ConsentDecisionMadeAndEEANoticeNotAcknowledged) {
  // If m1 consent decision has been made and the eea notice has not been
  // acknowledged, return kM1NoticeEEA.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kM1ConsentDecisionPreviouslyMade, true}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kM1NoticeEEA)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       M1ConsentDecisionMadeAndEEANoticeAcknowledged) {
  // If m1 consent decision has been made and the eea notice has been
  // acknowledged, return kNone.
  RunTestCase(TestState{{kM1PromptPreviouslySuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {kM1ConsentDecisionPreviouslyMade, true},
                        {kM1EEANoticePreviouslyAcknowledged, true}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, ROWNoticeAckTopicsDisabled) {
  // If the user saw the ROW notice, and then disable Topics from settings, and
  // is now in EEA, they should not see a prompt.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kM1RowNoticePreviouslyAcknowledged, true},
                {kM1TopicsEnabledUserPrefValue, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{
          {kPromptType, static_cast<int>(PromptType::kNone)},
          {kM1PromptSuppressedReason,
           static_cast<int>(
               PromptSuppressedReason::
                   kROWFlowCompletedAndTopicsDisabledBeforeEEAMigration)}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, PromptAction_ConsentAccepted) {
  // Confirm that when the service is informed that the consent prompt was
  // accepted, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(
      TestState{{kActiveTopicsConsent, false},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kPromptAction, static_cast<int>(PromptAction::kConsentAccepted)}},
      TestOutput{
          {kM1ConsentDecisionMade, true},
          {kM1TopicsEnabled, true},
          {kTopicsConsentGiven, true},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kConfirmation},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsConfirmationStringIdentifiers()}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest, PromptAction_ConsentDeclined) {
  // Confirm that when the service is informed that the consent prompt was
  // declined, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(
      TestState{{kActiveTopicsConsent, true},
                {kAdvanceClockBy, base::Hours(1)}},
      TestInput{
          {kPromptAction, static_cast<int>(PromptAction::kConsentDeclined)}},
      TestOutput{
          {kM1ConsentDecisionMade, true},
          {kM1TopicsEnabled, false},
          {kTopicsConsentGiven, false},
          {kTopicsConsentLastUpdateReason,
           privacy_sandbox::TopicsConsentUpdateSource::kConfirmation},
          {kTopicsConsentLastUpdateTime, base::Time::Now() + base::Hours(1)},
          {kTopicsConsentStringIdentifiers,
           GetTopicsConfirmationStringIdentifiers()}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       PromptAction_EEANoticeAcknowledged) {
  // Confirm that when the service is informed that the eea notice was
  // acknowledged, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(TestState{{kM1ConsentDecisionPreviouslyMade, true},
                        {kM1EEANoticePreviouslyAcknowledged, false}},
              TestInput{{kPromptAction,
                         static_cast<int>(PromptAction::kNoticeAcknowledge)}},
              TestOutput{{kM1EEANoticeAcknowledged, true},
                         {kM1FledgeEnabled, true},
                         {kM1AdMeasurementEnabled, true}});
  RunTestCase(
      TestState{{kM1ConsentDecisionPreviouslyMade, true},
                {kM1EEANoticePreviouslyAcknowledged, false}},
      TestInput{
          {kPromptAction, static_cast<int>(PromptAction::kNoticeOpenSettings)}},
      TestOutput{{kM1EEANoticeAcknowledged, true},
                 {kM1FledgeEnabled, true},
                 {kM1AdMeasurementEnabled, true},
                 {kTopicsConsentGiven, false},
                 {kTopicsConsentLastUpdateReason,
                  privacy_sandbox::TopicsConsentUpdateSource::kDefaultValue}});
}

TEST_F(PrivacySandboxServiceM1ConsentPromptTest,
       PromptAction_EEANoticeAcknowledged_ROWNoticeAcknowledged) {
  // Confirm that if the user has already acknowledged an ROW notice, that the
  // EEA notice does not attempt to re-enable APIs. This is important for the
  // ROW -> EEA upgrade flow, where the user may have already visited settings.
  RunTestCase(TestState{{kM1ConsentDecisionPreviouslyMade, true},
                        {kM1EEANoticePreviouslyAcknowledged, false},
                        {kM1RowNoticePreviouslyAcknowledged, true}},
              TestInput{{kPromptAction,
                         static_cast<int>(PromptAction::kNoticeAcknowledge)}},
              TestOutput{{kM1EEANoticeAcknowledged, true},
                         {kM1FledgeEnabled, false},
                         {kM1AdMeasurementEnabled, false}});
}

class PrivacySandboxServiceM1NoticePromptTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName,
          "false"},
         {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1NoticePromptTest, SuppressedNotice) {
  // A case that will normally show a notice will not if there is any
  // suppression reason.
  for (int suppressed_reason = static_cast<int>(PromptSuppressedReason::kNone);
       suppressed_reason <= static_cast<int>(PromptSuppressedReason::kMaxValue);
       ++suppressed_reason) {
    bool suppressed =
        suppressed_reason != static_cast<int>(PromptSuppressedReason::kNone);
    auto expected_prompt =
        suppressed ? PromptType::kNone : PromptType::kM1NoticeROW;
    RunTestCase(
        TestState{{kM1PromptPreviouslySuppressedReason, suppressed_reason}},
        TestInput{{kForceChromeBuild, true}},
        TestOutput{{kPromptType, static_cast<int>(expected_prompt)},
                   {kM1PromptSuppressedReason, suppressed_reason}});
  }
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, TrialsDisabledAfterNotice) {
  // If a previous notice was shown and then privacy sandbox was disabled after,
  // set kTrialsDisabledAfterNotice as suppressed reason and return kNone.
  // Now that the trials pref is deprecated users won't be able to enter that
  // state. Users who had the prompt suppressed due to declining the trials
  // consent should remain in this state.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(
                     PromptSuppressedReason::kTrialsDisabledAfterNotice)},
                {kTrialsNoticeDisplayed, true}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(
                      PromptSuppressedReason::kTrialsDisabledAfterNotice)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1NoticeNotAcknowledged) {
  // If m1 notice required, and the row notice has not been acknowledged, return
  // kM1NoticeROW.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kM1RowNoticePreviouslyAcknowledged, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kM1NoticeROW)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1NoticeAcknowledged) {
  // If m1 notice required, and the row notice has been acknowledged, return
  // kNone.
  RunTestCase(TestState{{kM1PromptPreviouslySuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {kM1RowNoticePreviouslyAcknowledged, true}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1EEAFlowInterrupted) {
  // If a user has migrated from EEA to ROW and has already completed the eea
  // consent but not yet acknowledged the notice, return kM1NoticeROW.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kM1ConsentDecisionPreviouslyMade, true},
                {kM1EEANoticePreviouslyAcknowledged, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kM1NoticeROW)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, M1EEAFlowCompleted) {
  // If a user has migrated from EEA to ROW and has already completed the eea
  // flow, set kEEAFlowCompleted as suppressed reason return kNone.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kM1ConsentDecisionPreviouslyMade, true},
                {kM1EEANoticePreviouslyAcknowledged, true}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{
          {kPromptType, static_cast<int>(PromptType::kNone)},
          {kM1PromptSuppressedReason,
           static_cast<int>(
               PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration)}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest,
       PromptAction_RowNoticeAcknowledged) {
  // Confirm that when the service is informed that the row notice was
  // acknowledged, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(TestState{},
              TestInput{{kPromptAction,
                         static_cast<int>(PromptAction::kNoticeAcknowledge)}},
              TestOutput{{kM1RowNoticeAcknowledged, true},
                         {kM1TopicsEnabled, true},
                         {kM1FledgeEnabled, true},
                         {kM1AdMeasurementEnabled, true},
                         {kTopicsConsentGiven, false}});
}

TEST_F(PrivacySandboxServiceM1NoticePromptTest, PromptAction_OpenSettings) {
  // Confirm that when the service is informed that the row notice was
  // acknowledged, it correctly adjusts the Privacy Sandbox prefs.
  RunTestCase(TestState{},
              TestInput{{kPromptAction,
                         static_cast<int>(PromptAction::kNoticeOpenSettings)}},
              TestOutput{{kM1RowNoticeAcknowledged, true},
                         {kM1TopicsEnabled, true},
                         {kM1FledgeEnabled, true},
                         {kM1AdMeasurementEnabled, true},
                         {kTopicsConsentGiven, false}});
}

TEST_F(PrivacySandboxServiceTest, DisablePrivacySandboxPromptPolicy) {
  // Disable the prompt via policy and check the returned prompt type is kNone.
  RunTestCase(TestState{{kM1PromptDisabledByPolicy,
                         static_cast<int>(PromptSuppressedReason::kPolicy)}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)}});
}

TEST_F(PrivacySandboxServiceTest, DisablePrivacySandboxTopicsPolicy) {
  // Disable the Topics api via policy and check the returned prompt type is
  // kNone and topics is not allowed.
  RunTestCase(TestState{{kM1TopicsDisabledByPolicy, true}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)},
                         {kIsTopicsAllowed, false}});
}

TEST_F(PrivacySandboxServiceTest, DisablePrivacySandboxFledgePolicy) {
  // Disable the Fledge api via policy and check the returned prompt type is
  // kNone and fledge is not allowed.
  RunTestCase(TestState{{kM1FledgeDisabledByPolicy, true}},
              TestInput{{kForceChromeBuild, true},
                        {kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kFledgeAuctionPartyOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)},
                         {kIsFledgeJoinAllowed, false}});
}

TEST_F(PrivacySandboxServiceTest, DisablePrivacySandboxAdMeasurementPolicy) {
  // Disable the ad measurement api via policy and check the returned prompt
  // type is kNone and the api is not allowed.
  RunTestCase(TestState{{kM1AdMesaurementDisabledByPolicy, true}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAdMeasurementReportingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))},
                        {kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)},
                         {kIsAttributionReportingAllowed, false}});
}

// TODO(crbug.com/40262246): consider parameterizing other tests for the various
// feature flags, particularly `kPrivacySandboxSettings4RestrictedNotice`.
class PrivacySandboxServiceM1RestrictedNoticePromptTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/true);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName,
          "false"},
         {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest, RestrictedNotice) {
  // Ensure that kM1NoticeRestricted is returned when configured to do so.
  RunTestCase(TestState{{kM1PromptPreviouslySuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {kTrialsNoticeDisplayed, false}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType,
                          static_cast<int>(PromptType::kM1NoticeRestricted)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       RestrictedNoticeAlreadyAcknowledged) {
  // If the user already acknowledged the notice, don't show it, or the ROW
  // notice, again.
  RunTestCase(TestState{{kM1PromptPreviouslySuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {kTrialsNoticeDisplayed, false},
                        {kM1RestrictedNoticePreviouslyAcknowledged, true}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       ROWNoticeAlreadyAcknowledged) {
  // If the user already acknowledged a different notice, don't show it again.
  RunTestCase(TestState{{kM1PromptPreviouslySuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {kTrialsNoticeDisplayed, false},
                        {kM1RowNoticePreviouslyAcknowledged, true}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       EEANoticeAlreadyAcknowledged) {
  // If the user already acknowledged a different notice, don't show the
  // restricted notice again. Ensure the existing suppression reason is
  // respected.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kTrialsNoticeDisplayed, false},
                {kM1ConsentDecisionPreviouslyMade, true},
                {kM1EEANoticePreviouslyAcknowledged, true}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{
          {kPromptType, static_cast<int>(PromptType::kNone)},
          {kM1PromptSuppressedReason,
           static_cast<int>(
               PromptSuppressedReason::kEEAFlowCompletedBeforeRowMigration)}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kNone));

  base::test::ScopedFeatureList feature_list_notice_required;
  std::map<std::string, std::string> notice_required_feature_param = {
      {std::string(
           privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName),
       "false"},
      {std::string(privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName),
       "true"}};
  feature_list_notice_required.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4, notice_required_feature_param);

  // Notice flow not completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      false);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kRestrictedNoticePromptWaiting),
      /*expected_count=*/1);

  // Notice flow completed.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kRestrictedNoticeFlowCompleted),
      /*expected_count=*/1);

  // ROW flow completed, which implies no restricted prompt.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      false);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RowNoticeAcknowledged, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxServiceImpl::PromptStartupState::
              kRestrictedNoticeNotShownDueToFullNoticeAcknowledged),
      /*expected_count=*/1);

  // EAA flow completed, which implies no restricted prompt.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                      false);
  prefs()->SetBoolean(prefs::kPrivacySandboxM1EEANoticeAcknowledged, true);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxServiceImpl::PromptStartupState::
              kRestrictedNoticeNotShownDueToFullNoticeAcknowledged),
      // One when the ROW notice acknowledged pref was set, plus the latest
      // call.
      /*expected_count=*/2);
}

class PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted
    : public PrivacySandboxServiceM1RestrictedNoticePromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/true);
    mock_delegate->SetUpIsPrivacySandboxCurrentlyUnrestrictedResponse(
        /*is_unrestricted=*/true);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName,
          "false"},
         {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
       RecordPrivacySandbox4StartupMetrics_GraduationFlow) {
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kNone));

  // Restricted Notice flow NOT completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        false);
    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(
            PrivacySandboxServiceImpl::PromptStartupState::
                kWaitingForGraduationRestrictedNoticeFlowNotCompleted),
        /*expected_count=*/1);
  }

  // Restricted Notice flow completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        true);

    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(
            PrivacySandboxServiceImpl::PromptStartupState::
                kWaitingForGraduationRestrictedNoticeFlowCompleted),
        /*expected_count=*/1);
  }
}

TEST_F(
    PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyUnrestricted,
    RecordPrivacySandbox4StartupMetrics_GraduationFlowWhenNoticeShownToGuardian) {
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  base::HistogramTester histogram_tester;

  // User was reported restricted
  prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

  // Prompt is suppressed because direct notice was shown to guardian
  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian));

  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(
          PrivacySandboxServiceImpl::PromptStartupState::
              kWaitingForGraduationRestrictedNoticeFlowNotCompleted),
      /*expected_count=*/1);
}

class PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyRestricted
    : public PrivacySandboxServiceM1RestrictedNoticePromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/true);
    mock_delegate->SetUpIsPrivacySandboxCurrentlyUnrestrictedResponse(
        /*is_unrestricted=*/false);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName,
          "false"},
         {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeUserCurrentlyRestricted,
       RecordPrivacySandbox4StartupMetrics_GraduationFlow) {
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";

  // Ensure prompt not suppressed.
  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kNone));

  // Restricted Notice flow completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        true);
    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                             kRestrictedNoticeFlowCompleted),
        /*expected_count=*/1);
  }

  // Restricted Notice flow NOT completed
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
                        false);
    // User was reported restricted
    prefs()->SetBoolean(prefs::kPrivacySandboxM1Restricted, true);

    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram,
        static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                             kRestrictedNoticePromptWaiting),
        /*expected_count=*/1);
  }
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticePromptTest,
       RestrictedNoticeAcknowledged) {
  // Ensure that Ad measurement pref is not re-enabled if user disabled it
  // after acknowledging the restricted notice.
  RunTestCase(TestState{{kM1PromptPreviouslySuppressedReason,
                         static_cast<int>(PromptSuppressedReason::kNone)},
                        {kM1RestrictedNoticePreviouslyAcknowledged, true},
                        {kM1AdMeasurementEnabledUserPrefValue, false}},
              TestInput{{kForceChromeBuild, true}},
              TestOutput{{kPromptType, static_cast<int>(PromptType::kNone)},
                         {kM1AdMeasurementEnabled, false},
                         {kM1PromptSuppressedReason,
                          static_cast<int>(PromptSuppressedReason::kNone)}});
}

class PrivacySandboxServiceM1RestrictedNoticeShownToGuardianTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/true);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName,
          "false"},
         {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeShownToGuardianTest,
       NotSubjectToNoticeButIsRestricted) {
  // Ensure that kNoticeShownToGuardian, with no prompt, is returned in the
  // event that the user is not subject to the m1 notice restricted prompt.
  // Ensure measurements API is enabled for these users.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kTrialsNoticeDisplayed, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{
          {kPromptType, static_cast<int>(PromptType::kNone)},
          {kM1PromptSuppressedReason,
           static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian)},
          {kM1AdMeasurementEnabled, true}});
}

TEST_F(PrivacySandboxServiceM1RestrictedNoticeShownToGuardianTest,
       NotSubjectToNoticeButIsRestrictedWithAdMeasurementDisabled) {
  // Ensure that Ad measurement pref is not re-enabled if user disabled it
  // after the notice was suppressed due to kNoticeShownToGuardian.
  RunTestCase(
      TestState{
          {kM1PromptPreviouslySuppressedReason,
           static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian)},
          {kM1AdMeasurementEnabledUserPrefValue, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{
          {kPromptType, static_cast<int>(PromptType::kNone)},
          {kM1AdMeasurementEnabled, false},
          {kM1PromptSuppressedReason,
           static_cast<int>(PromptSuppressedReason::kNoticeShownToGuardian)}});
}

class PrivacySandboxServiceM1RestrictedNoticeEnabledNoRestrictionsTest
    : public PrivacySandboxServiceM1PromptTest {
 public:
  std::unique_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    mock_delegate->SetUpIsSubjectToM1NoticeRestrictedResponse(
        /*is_subject_to_restricted_notice=*/false);
    return mock_delegate;
  }
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxSettings4,
        {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName,
          "false"},
         {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"},
         {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
          "true"}});
  }
};

TEST_F(PrivacySandboxServiceM1RestrictedNoticeEnabledNoRestrictionsTest,
       VerifyPromptType) {
  // The restricted notice feature is enabled, but the account is not subject to
  // the restrictions, and the privacy sandbox is not otherwise restricted. The
  // ROW notice is still applicable, however.
  RunTestCase(
      TestState{{kM1PromptPreviouslySuppressedReason,
                 static_cast<int>(PromptSuppressedReason::kNone)},
                {kTrialsNoticeDisplayed, false}},
      TestInput{{kForceChromeBuild, true}},
      TestOutput{{kPromptType, static_cast<int>(PromptType::kM1NoticeROW)},
                 {kM1PromptSuppressedReason,
                  static_cast<int>(PromptSuppressedReason::kNone)}});
}

#if BUILDFLAG(IS_ANDROID)
class PrivacySandboxActivityTypeStorageTests
    : public PrivacySandboxServiceTest {
 public:
  PrivacySandboxActivityTypeStorageTests()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}

  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxActivityTypeStorage,
        {{"last-n-launches", "5"},
         {"within-x-days", "2"},
         {"skip-pre-first-tab", "false"}});
  }

 protected:
  base::HistogramTester histogram_tester;
  ScopedTestingLocalState* local_state() { return local_state_.get(); }

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyListOverflow) {
  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            3u);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebApk);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            4u);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebapp);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            5u);
  //   Since we are already at a size of 5, and last-n-launches is set to 5, the
  //   next call of another launch will remove the first element in the list
  //   before adding the newly created one. The size should still be 5.
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            5u);
}

// This test is ensuring that the start of the list is represented as the newest
// records and the end is the oldest records.
TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyListOrder) {
  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kAGSACustomTab));

  browser_task_environment()->FastForwardBy(base::Minutes(5));
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kNonAGSACustomTab));

  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTrustedWebActivity));

  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebapp);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kWebapp));

  browser_task_environment()->FastForwardBy(base::Minutes(5));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebApk);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kAGSACustomTab));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[1]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kWebApk));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[2]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kWebapp));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[3]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTabbed));
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[4]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTrustedWebActivity));
}

TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyListExpiration) {
  privacy_sandbox_service()->RecordActivityType(ActivityType::kOther);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);
  // Even though within-x-days is set to 2 days, we still include records that
  // are inclusive of the time boundary. When we fast forward by 2 days and add
  // a third record, all three entries are still in the record list.
  browser_task_environment()->FastForwardBy(base::Days(2));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kPreFirstTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            3u);
  // Now by fast forwarding by 1 more day, we have exceeded the within-x-days of
  // 2 days, so the first two entries should be removed and the size should
  // be 2.
  browser_task_environment()->FastForwardBy(base::Days(1));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebApk);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);
}

TEST_F(PrivacySandboxActivityTypeStorageTests, VerifyTimeBackwards) {
  // Initializing the activity type record list with entries that have
  // timestamps set for future dates (e.g., 5 and 7 days from now).
  base::Value::List old_records;
  base::Value::Dict first_record;
  base::Value::Dict second_record;

  first_record.Set("timestamp",
                   base::TimeToValue(base::Time::Now() + base::Days(5)));
  first_record.Set("activity_type",
                   static_cast<int>(ActivityType::kAGSACustomTab));

  second_record.Set("timestamp",
                    base::TimeToValue(base::Time::Now() + base::Days(7)));
  second_record.Set("activity_type", static_cast<int>(ActivityType::kTabbed));

  old_records.Append(std::move(first_record));
  old_records.Append(std::move(second_record));

  prefs()->SetList(prefs::kPrivacySandboxActivityTypeRecord2,
                   std::move(old_records));

  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            2u);

  // After recording a new activity, any previous records with timestamps in the
  // future (greater than the current timestamp) are not added to the updated
  // list.
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTrustedWebActivity));
}

class PrivacySandboxActivityTypeStorageMetricsTests
    : public PrivacySandboxServiceTest {
 public:
  PrivacySandboxActivityTypeStorageMetricsTests()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {}

  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxActivityTypeStorage,
        {{"last-n-launches", "100"},
         {"within-x-days", "60"},
         {"skip-pre-first-tab", "false"}});
  }

  struct PercentageMetricValues {
    int AGSACCTPercent = 0;
    int AGSACCTBucketCount = 1;
    int BrAppPercent = 0;
    int BrAppBucketCount = 1;
    int NonAGSACCTPercent = 0;
    int NonAGSACCTBucketCount = 1;
    int TWAPercent = 0;
    int TWABucketCount = 1;
    int WebappPercent = 0;
    int WebappBucketCount = 1;
    int WebAPKPercent = 0;
    int WebAPKBucketCount = 1;
    int OtherPercent = 0;
    int OtherBucketCount = 1;
    int PreFirstTabPercent = 0;
    int PreFirstTabCount = 1;
  };

  void TestMetricValues(PercentageMetricValues values) {
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.AGSACCT2",
        values.AGSACCTPercent, values.AGSACCTBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2",
        values.BrAppPercent, values.BrAppBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.NonAGSACCT2",
        values.NonAGSACCTPercent, values.NonAGSACCTBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.TWA2", values.TWAPercent,
        values.TWABucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.WebApp2",
        values.WebappPercent, values.WebappBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.WebApk2",
        values.WebAPKPercent, values.WebAPKBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.Other2",
        values.OtherPercent, values.OtherBucketCount);
    histogram_tester.ExpectBucketCount(
        "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2",
        values.PreFirstTabPercent, values.PreFirstTabCount);
  }

 protected:
  base::HistogramTester histogram_tester;
  ScopedTestingLocalState* local_state() { return local_state_.get(); }

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyMetricsRecordsLength) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 1, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebapp);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 2, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebApk);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 3, 1);
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 4, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 5, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kOther);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 6, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kPreFirstTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength", 7, 1);
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyMetricsPercentages) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  TestMetricValues({.AGSACCTPercent = 100});

  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  TestMetricValues({.AGSACCTPercent = 50,
                    .BrAppBucketCount = 2,
                    .NonAGSACCTPercent = 50,
                    .TWABucketCount = 2,
                    .WebappBucketCount = 2,
                    .WebAPKBucketCount = 2,
                    .OtherBucketCount = 2,
                    .PreFirstTabCount = 2});

  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  TestMetricValues({.AGSACCTPercent = 33,
                    .BrAppBucketCount = 3,
                    .NonAGSACCTPercent = 33,
                    .TWAPercent = 33,
                    .WebappBucketCount = 3,
                    .WebAPKBucketCount = 3,
                    .OtherBucketCount = 3,
                    .PreFirstTabCount = 3});

  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  TestMetricValues({.AGSACCTPercent = 50,
                    .AGSACCTBucketCount = 2,
                    .BrAppBucketCount = 4,
                    .NonAGSACCTPercent = 25,
                    .TWAPercent = 25,
                    .WebappBucketCount = 4,
                    .WebAPKBucketCount = 4,
                    .OtherBucketCount = 4,
                    .PreFirstTabCount = 4});

  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebApk);
  TestMetricValues({.AGSACCTPercent = 40,
                    .BrAppBucketCount = 5,
                    .NonAGSACCTPercent = 20,
                    .TWAPercent = 20,
                    .WebappBucketCount = 5,
                    .WebAPKPercent = 20,
                    .OtherBucketCount = 5,
                    .PreFirstTabCount = 5});

  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  TestMetricValues({.AGSACCTPercent = 33,
                    .AGSACCTBucketCount = 2,
                    .BrAppBucketCount = 6,
                    .NonAGSACCTPercent = 17,
                    .TWAPercent = 33,
                    .TWABucketCount = 2,
                    .WebappBucketCount = 6,
                    .WebAPKPercent = 17,
                    .OtherBucketCount = 6,
                    .PreFirstTabCount = 6});

  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebapp);
  TestMetricValues({.AGSACCTPercent = 29,
                    .BrAppBucketCount = 7,
                    .NonAGSACCTPercent = 14,
                    .TWAPercent = 29,
                    .WebappPercent = 14,
                    .WebAPKPercent = 14,
                    .OtherBucketCount = 7,
                    .PreFirstTabCount = 7});

  browser_task_environment()->FastForwardBy(base::Days(61));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  // Since 61 days have passed, the activity log gets cleared because it is
  // passed our within-x-days feature param.
  TestMetricValues({.BrAppPercent = 100,
                    .NonAGSACCTBucketCount = 2,
                    .TWABucketCount = 3,
                    .WebappBucketCount = 7,
                    .WebAPKBucketCount = 5,
                    .OtherBucketCount = 8,
                    .PreFirstTabCount = 8});
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyUserSegmentMetrics) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  for (int i = 0; i < 10; ++i) {
    privacy_sandbox_service()->RecordActivityType(ActivityType::kWebapp);
  }
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2", 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasWebapp, 1);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2", 0);

  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasTWA, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebapp);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasTWA, 2);

  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebApk);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasPWA, 1);
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kTrustedWebActivity);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasPWA, 2);

  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasNonAGSACCT, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kWebApk);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasNonAGSACCT, 2);

  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasAGSACCT, 1);
  privacy_sandbox_service()->RecordActivityType(
      ActivityType::kNonAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasAGSACCT, 2);

  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 2);

  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2", 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 1);

  for (int i = 0; i < 9; ++i) {
    privacy_sandbox_service()->RecordActivityType(ActivityType::kAGSACustomTab);
  }
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 10);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasAGSACCT, 3);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasBrowserApp, 10);

  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasOther, 0);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasOther, 0);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2",
      UserSegment::kHasPreFirstTab, 0);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2",
      UserSegment::kHasPreFirstTab, 0);
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests, VerifyNoMetrics) {
  // Set the kMetricsReportingEnabledTimestamp of UMA opt in to 10 days in the
  // future and we should receive no metrics on any of the data in the Activity
  // Type storage list. The list should still be populated to a size of 10
  // records.
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() + base::Days(10)).ToTimeT());
  for (int i = 0; i < 10; ++i) {
    privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  }
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.10MostRecentRecordsUserSegment2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.20MostRecentRecordsUserSegment2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.AGSACCT2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.NonAGSACCT2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.TWA2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.WebApp2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.Other2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2", 0);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.ActivityTypeStorage.RecordsLength2", 0);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            10u);
}

TEST_F(PrivacySandboxActivityTypeStorageMetricsTests,
       VerifyDurationSinceOldestRecordMetrics) {
  local_state()->Get()->SetInt64(
      metrics::prefs::kMetricsReportingEnabledTimestamp,
      (base::Time::Now() - base::Days(10)).ToTimeT());
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 0, 1);
  browser_task_environment()->FastForwardBy(base::Days(5));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 5, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 5, 2);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 15, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 25, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 35, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 45, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 55, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 60, 1);
  browser_task_environment()->FastForwardBy(base::Days(10));
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.DaysSinceOldestRecord", 60, 2);
}

class PrivacySandboxActivityTypeStorageMetricsTypeReceivedTests
    : public PrivacySandboxActivityTypeStorageMetricsTests,
      public testing::WithParamInterface<int> {};

TEST_P(PrivacySandboxActivityTypeStorageMetricsTypeReceivedTests,
       VerifyTypeReceivedMetric) {
  privacy_sandbox_service()->RecordActivityType(
      static_cast<ActivityType>(GetParam()));
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived",
      static_cast<ActivityType>(GetParam()), 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrivacySandboxActivityTypeStorageMetricsTypeReceivedTests,
    testing::Range(static_cast<int>(ActivityType::kOther),
                   static_cast<int>(ActivityType::kMaxValue) + 1));

class PrivacySandboxActivityTypeStorageSkipPreFirstTabTests
    : public PrivacySandboxActivityTypeStorageTests {
  void InitializeFeaturesBeforeStart() override {
    feature_list()->InitAndEnableFeatureWithParameters(
        privacy_sandbox::kPrivacySandboxActivityTypeStorage,
        {{"last-n-launches", "100"},
         {"within-x-days", "60"},
         {"skip-pre-first-tab", "true"}});
  }
};

TEST_F(PrivacySandboxActivityTypeStorageSkipPreFirstTabTests,
       RecordsOnlyTabbedActivity) {
  privacy_sandbox_service()->RecordActivityType(ActivityType::kTabbed);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived", ActivityType::kTabbed,
      1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2", 100, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2", 0, 1);
  privacy_sandbox_service()->RecordActivityType(ActivityType::kPreFirstTab);
  EXPECT_EQ(prefs()->GetList(prefs::kPrivacySandboxActivityTypeRecord2).size(),
            1u);
  EXPECT_EQ(*prefs()
                 ->GetList(prefs::kPrivacySandboxActivityTypeRecord2)[0]
                 .GetDict()
                 .Find("activity_type"),
            static_cast<int>(ActivityType::kTabbed));
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.TypeReceived",
      ActivityType::kPreFirstTab, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.BrApp2", 100, 1);
  histogram_tester.ExpectBucketCount(
      "PrivacySandbox.ActivityTypeStorage.Percentage.PreFirstTab2", 0, 1);
}
#endif  // BUILDFLAG(IS_ANDROID)
