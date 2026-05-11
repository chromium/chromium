// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <tuple>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/profile_bucket_metrics.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browsing_topics/test_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace {
using ::browsing_topics::Topic;
using ::privacy_sandbox::CanonicalTopic;

using EligibilityLevel = ::privacy_sandbox::EligibilityLevel;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::ValuesIn;

using enum privacy_sandbox_test_util::StateKey;
using enum privacy_sandbox_test_util::InputKey;
using enum privacy_sandbox_test_util::OutputKey;

using privacy_sandbox_test_util::InputKey;
using privacy_sandbox_test_util::OutputKey;
using privacy_sandbox_test_util::StateKey;

using privacy_sandbox_test_util::TestCase;
using privacy_sandbox_test_util::TestInput;
using privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::TestState;

constexpr char kFirstPartySetsStateHistogram[] =
    "Settings.FirstPartySets.State";
constexpr char kTrackingProtectionStateHistogram[] =
    "Settings.TrackingProtection.Enabled";
constexpr char kDefaultProfileUsername[] = "user@gmail.com";
constexpr char kTestEmail[] = "test@test.com";

const base::Version& GetRelatedWebsiteSetsVersion() {
  static const base::NoDestructor<base::Version> kVersion("1.2.3");
  return *kVersion;
}

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
    NOTREACHED();
  }
  void GetAllInterestGroupDataKeys(
      base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback)
      override {
    std::move(callback).Run(data_keys_);
  }
  void RemoveInterestGroupsByDataKey(InterestGroupDataKey data_key,
                                     base::OnceClosure callback) override {
    NOTREACHED();
  }
  void AddTrustedServerKeysDebugOverride(
      TrustedServerAPIType api,
      const url::Origin& coordinator,
      std::string serialized_keys,
      base::OnceCallback<void(std::optional<std::string>)> callback) override {
    NOTREACHED();
  }

 private:
  std::vector<InterestGroupDataKey> data_keys_;
};

// Remove any user preference settings for Related Website Set related
// preferences, returning them to their default value.
void ClearRwsUserPrefs(
    sync_preferences::TestingPrefServiceSyncable* pref_service) {
  pref_service->RemoveUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
  pref_service->RemoveUserPref(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized);
}

std::vector<int> GetTopicsSettingsStringIdentifiers(bool did_consent,
                                                    bool has_current_topics,
                                                    bool has_blocked_topics) {
  if (did_consent && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
            IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_V2,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (did_consent && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
            IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_DISABLED,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_NEW,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && has_current_topics && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
            IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_NEW,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && has_current_topics && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
            IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_V2,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && !has_current_topics && has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
            IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_V2,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_NEW,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  } else if (!did_consent && !has_current_topics && !has_blocked_topics) {
    return {IDS_SETTINGS_TOPICS_PAGE_TITLE,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_LABEL,
            IDS_SETTINGS_TOPICS_PAGE_TOGGLE_SUB_LABEL_V2,
            IDS_SETTINGS_TOPICS_PAGE_ACTIVE_TOPICS_HEADING,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL,
            IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_EMPTY_TEXT_V2,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_HEADING_NEW,
            IDS_SETTINGS_TOPICS_PAGE_BLOCKED_TOPICS_DESCRIPTION_EMPTY_TEXT_V2,
            IDS_SETTINGS_TOPICS_PAGE_FOOTER_CANONICAL};
  }

  NOTREACHED() << "Invalid topics settings consent state";
}
}  // namespace

// A mock implementation of the PrivacySandboxCountries interface for testing.
class MockPrivacySandboxCountries : public PrivacySandboxCountries {
 public:
  MOCK_METHOD(bool, IsConsentCountry, (), (override));
  MOCK_METHOD(bool, IsRestOfWorldCountry, (), (override));
  MOCK_METHOD(bool, IsLatestCountryChina, (), (override));
};

class PrivacySandboxServiceTest : public testing::Test {
 public:
  PrivacySandboxServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_attestations_(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting()) {
    CreateDefaultProfile();
    first_party_sets_policy_service_ =
        std::make_unique<first_party_sets::FirstPartySetsPolicyService>(
            profile()->GetOriginalProfile());
  }

  void SetUp() override {
    InitializeFeaturesBeforeStart();
    CreateService();

    base::RunLoop run_loop;
    first_party_sets_policy_service_->WaitForFirstInitCompleteForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
    first_party_sets_policy_service_->ResetForTesting();
  }

  void TearDown() override {
    privacy_sandbox_service_->Shutdown();
    privacy_sandbox_service_ = nullptr;
  }

  virtual void InitializeFeaturesBeforeStart() {}

  void CreateDefaultProfile() {
    default_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(default_profile_manager_->SetUp());

    default_profile_ = default_profile_manager_->CreateTestingProfile(
        kDefaultProfileUsername, IdentityTestEnvironmentProfileAdaptor::
                                     GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            default_profile_.get());
    identity_test_env_adaptor_->identity_test_env()
        ->EnableRemovalOfExtendedAccountInfo();
  }

  void EnableSignIn() {
    auto account_info = identity_test_env_adaptor_->identity_test_env()
                            ->MakePrimaryAccountAvailable(
                                kTestEmail, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    signin::UpdateAccountInfoForAccount(
        identity_test_env_adaptor_->identity_test_env()->identity_manager(),
        account_info);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  void EnableSignInU18() {
    auto account_info = identity_test_env_adaptor_->identity_test_env()
                            ->MakePrimaryAccountAvailable(
                                kTestEmail, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_run_chrome_privacy_sandbox_trials(false);
    signin::UpdateAccountInfoForAccount(
        identity_test_env_adaptor_->identity_test_env()->identity_manager(),
        account_info);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  void EnableSignInOver18() {
    auto account_info = identity_test_env_adaptor_->identity_test_env()
                            ->MakePrimaryAccountAvailable(
                                kTestEmail, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_run_chrome_privacy_sandbox_trials(true);
    signin::UpdateAccountInfoForAccount(
        identity_test_env_adaptor_->identity_test_env()->identity_manager(),
        account_info);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

// ChromeOS users cannot sign out, their account preferences can never be
// cleared.
#if !BUILDFLAG(IS_CHROMEOS)

  void SignOut() {
    identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();
  }

#endif  // !BUILDFLAG(IS_CHROMEOS)

  virtual std::unique_ptr<
      privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
  CreateMockDelegate() {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    return mock_delegate;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  void CreateService() {
    // `CreateService` is sometimes called twice, or more in a tests.
    // Previous instances must be destroyed in the opposite order of their
    // construction.
    if (privacy_sandbox_service_) {
      privacy_sandbox_service_->Shutdown();
      privacy_sandbox_service_ = nullptr;
    }

    auto mock_delegate = CreateMockDelegate();
    mock_delegate_ = mock_delegate.get();
    mock_privacy_sandbox_countries_ =
        std::make_unique<MockPrivacySandboxCountries>();

    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettingsImpl>(
            std::move(mock_delegate), host_content_settings_map(),
            cookie_settings(), prefs());

    privacy_sandbox_service_ =
        PrivacySandboxServiceFactory::GetInstance()
            ->SetTestingSubclassFactoryAndUse(
                profile(),
                base::BindOnce(&PrivacySandboxServiceTest::BuildTestService,
                               base::Unretained(this)));
  }

  virtual profile_metrics::BrowserProfileType GetProfileType() {
    return profile_type_;
  }

  void SetProfileType(profile_metrics::BrowserProfileType profile_type) {
    profile_type_ = profile_type;
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

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }
  TestingProfile* profile() { return default_profile_; }
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
    return first_party_sets_policy_service_.get();
  }

  MockPrivacySandboxCountries* mock_privacy_sandbox_countries() {
    return mock_privacy_sandbox_countries_.get();
  }

  void MoveToEEA() {
    ON_CALL(*mock_privacy_sandbox_countries(), IsConsentCountry())
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_privacy_sandbox_countries(), IsRestOfWorldCountry())
        .WillByDefault(testing::Return(false));
  }

  void MoveToROW() {
    ON_CALL(*mock_privacy_sandbox_countries(), IsConsentCountry())
        .WillByDefault(testing::Return(false));
    ON_CALL(*mock_privacy_sandbox_countries(), IsRestOfWorldCountry())
        .WillByDefault(testing::Return(true));
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  content::BrowserTaskEnvironment* browser_task_environment() {
    return &browser_task_environment_;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<PrivacySandboxServiceImpl> BuildTestService(
      content::BrowserContext* context) {
    return std::make_unique<PrivacySandboxServiceImpl>(
        profile(), privacy_sandbox_settings(), cookie_settings(),
        profile()->GetPrefs(), test_interest_group_manager(), GetProfileType(),
        browsing_data_remover(), host_content_settings_map(),
        mock_browsing_topics_service(), first_party_sets_policy_service(),
        mock_privacy_sandbox_countries());
  }

  content::BrowserTaskEnvironment browser_task_environment_;

  // In production, ProfileManager is created much earlier than Profile
  // creation.
  std::unique_ptr<TestingProfileManager> default_profile_manager_;
  raw_ptr<TestingProfile> default_profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  profile_metrics::BrowserProfileType profile_type_ =
      profile_metrics::BrowserProfileType::kRegular;

  base::test::ScopedFeatureList outer_feature_list_;
  base::test::ScopedFeatureList inner_feature_list_;
  TestInterestGroupManager test_interest_group_manager_;
  browsing_topics::MockBrowsingTopicsService mock_browsing_topics_service_;

  first_party_sets::ScopedMockFirstPartySetsHandler
      mock_first_party_sets_handler_;
  std::unique_ptr<first_party_sets::FirstPartySetsPolicyService>
      first_party_sets_policy_service_;
  std::unique_ptr<MockPrivacySandboxCountries> mock_privacy_sandbox_countries_;
  std::unique_ptr<privacy_sandbox::PrivacySandboxSettings>
      privacy_sandbox_settings_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
      mock_delegate_;  // Owned by |privacy_sandbox_settings_|.
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations_;

  raw_ptr<PrivacySandboxServiceImpl> privacy_sandbox_service_ = nullptr;
};

class PrivacySandboxServiceAdPrivacyUxDeprecationTest
    : public PrivacySandboxServiceTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kPrivacySandboxAdPrivacyUxDeprecation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PrivacySandboxServiceAdPrivacyUxDeprecationTest, TopicsDataCleared) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(1);
  CreateService();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
}

TEST_F(PrivacySandboxServiceAdPrivacyUxDeprecationTest, FledgeDataCleared) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  ASSERT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            uint64_t(-1));
  CreateService();
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  uint64_t expected_fledge_mask =
      content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS |
      content::BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE |
      content::BrowsingDataRemover::DATA_TYPE_INTEREST_GROUPS_INTERNAL;
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            expected_fledge_mask);
}

TEST_F(PrivacySandboxServiceAdPrivacyUxDeprecationTest,
       AdMeasurementDataCleared) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  ASSERT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            uint64_t(-1));
  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
  uint64_t expected_measurement_mask =
      content::BrowsingDataRemover::DATA_TYPE_ATTRIBUTION_REPORTING |
      content::BrowsingDataRemover::DATA_TYPE_AGGREGATION_SERVICE |
      content::BrowsingDataRemover::DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL;
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            expected_measurement_mask);
}

class PrivacySandboxServiceAdPrivacyUxDeprecationDisabledTest
    : public PrivacySandboxServiceTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list_.InitAndDisableFeature(
        privacy_sandbox::kPrivacySandboxAdPrivacyUxDeprecation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PrivacySandboxServiceAdPrivacyUxDeprecationDisabledTest,
       TopicsDataNotCleared) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
  EXPECT_CALL(*mock_browsing_topics_service(), ClearAllTopicsData()).Times(0);
  CreateService();
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
}

TEST_F(PrivacySandboxServiceAdPrivacyUxDeprecationDisabledTest,
       FledgeDataNotCleared) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
  CreateService();
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            uint64_t(-1));
}

TEST_F(PrivacySandboxServiceAdPrivacyUxDeprecationDisabledTest,
       AdMeasurementDataNotCleared) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
  EXPECT_EQ(browsing_data_remover()->GetLastUsedRemovalMaskForTesting(),
            uint64_t(-1));
}

class PrivacySandboxPrivacyGuideShouldShowAdTopicsTest
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<bool> {};

TEST_P(PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
       ShownAccordingToConsentCountryAndFeature) {
  bool is_consent_country = GetParam();

  ON_CALL(*mock_privacy_sandbox_countries(), IsConsentCountry())
      .WillByDefault(testing::Return(is_consent_country));

  bool should_show_card =
      privacy_sandbox_service()
          ->PrivacySandboxPrivacyGuideShouldShowAdTopicsCard();
  // The expected result is identical to the consent country status.
  ASSERT_EQ(should_show_card, is_consent_country);
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
                         PrivacySandboxPrivacyGuideShouldShowAdTopicsTest,
                         testing::Bool());

class PrivacySandboxShouldUsePrivacyPolicyChinaDomain
    : public PrivacySandboxServiceTest {};

TEST_F(PrivacySandboxShouldUsePrivacyPolicyChinaDomain, ShouldUseChinaDomain) {
  ON_CALL(*mock_privacy_sandbox_countries(), IsLatestCountryChina())
      .WillByDefault(testing::Return(true));

  bool should_use_china_domain =
      privacy_sandbox_service()->ShouldUsePrivacyPolicyChinaDomain();
  ASSERT_EQ(should_use_china_domain, true);
}

TEST_F(PrivacySandboxShouldUsePrivacyPolicyChinaDomain,
       ShouldNotUseChinaDomain) {
  ON_CALL(*mock_privacy_sandbox_countries(), IsLatestCountryChina())
      .WillByDefault(testing::Return(false));

  bool should_use_china_domain =
      privacy_sandbox_service()->ShouldUsePrivacyPolicyChinaDomain();
  ASSERT_EQ(should_use_china_domain, false);
}

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


TEST_F(PrivacySandboxServiceTest,
       RelatedWebsiteSetsNotRelevantMetricAllowedCookies) {
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
       RelatedWebsiteSetsNotRelevantMetricBlockedCookies) {
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

TEST_F(PrivacySandboxServiceTest, RelatedWebsiteSetsEnabledMetric) {
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

TEST_F(PrivacySandboxServiceTest, RelatedWebsiteSetsDisabledMetric) {
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

class TrackingProtectionHistogramTest
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(TrackingProtectionHistogramTest,
                         TrackingProtectionHistogramTest,
                         testing::Bool());

TEST_P(TrackingProtectionHistogramTest, HistogramReflectsPref) {
  base::HistogramTester histogram_tester;
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, GetParam());
  CreateService();
  histogram_tester.ExpectUniqueSample(kTrackingProtectionStateHistogram,
                                      GetParam(), 1);
}

TEST_F(PrivacySandboxServiceTest,
       GetRelatedWebsiteSetOwner_SimulatedRwsData_DisabledWhen3pcAllowed) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global RWS with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets =
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {primary_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kPrimary)}},
              {associate1_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {});

  // Simulate 3PC are allowed while RWS pref is enabled
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since RWS is disabled.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
}

TEST_F(
    PrivacySandboxServiceTest,
    GetRelatedWebsiteSetOwner_SimulatedRwsData_DisabledWhenAllCookiesBlocked) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global RWS with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets =
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {primary_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kPrimary)}},
              {associate1_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {});

  // Simulate all cookies are blocked while RWS pref is enabled
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since RWS is disabled.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetRelatedWebsiteSetOwner_SimulatedRwsData_DisabledByRwsPref) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global RWS with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets =
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {primary_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kPrimary)}},
              {associate1_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {});

  // Simulate RWS pref disabled while 3PC are being blocked
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(false));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();

  // We shouldn't get associate1's owner since RWS is disabled.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       SimulatedRwsData_RwsEnabled_WithoutGlobalSets) {
  GURL primary_gurl("https://primary.test");
  GURL associate1_gurl("https://associate1.test");
  GURL associate2_gurl("https://associate2.test");
  net::SchemefulSite primary_site(primary_gurl);
  net::SchemefulSite associate1_site(associate1_gurl);
  net::SchemefulSite associate2_site(associate2_gurl);

  // Set up state for the RWS UI: block 3PC and enable the RWS pref.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Verify `GetRelatedWebsiteSetOwner` returns empty if RWS is enabled but the
  // Global sets are not ready yet.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate2_gurl),
      std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       SimulatedRwsData_RwsEnabled_WithGlobalSetsAndProfileSets) {
  GURL primary_gurl("https://primary.test");
  GURL associate1_gurl("https://associate1.test");
  GURL associate2_gurl("https://associate2.test");
  net::SchemefulSite primary_site(primary_gurl);
  net::SchemefulSite associate1_site(associate1_gurl);
  net::SchemefulSite associate2_site(associate2_gurl);

  // Set up state for the RWS UI: block 3PC and enable the RWS pref.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Simulate that the Global RWS are ready with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test", "https://associate2.test"] }
  mock_first_party_sets_handler().SetGlobalSets(
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {primary_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kPrimary)}},
              {associate1_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
              {associate2_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {}));

  // Simulate that associate2 is removed from the Global RWS for
  // this profile.
  mock_first_party_sets_handler().SetContextConfig(
      net::FirstPartySetsContextConfig::Create(
          {{net::SchemefulSite(GURL("https://associate2.test")),
            net::FirstPartySetEntryOverride()}})
          .value());

  first_party_sets_policy_service()->InitForTesting();

  // Verify that primary owns associate1, but no longer owns associate2.
  EXPECT_EQ(privacy_sandbox_service()
                ->GetRelatedWebsiteSetOwner(associate1_gurl)
                .value(),
            primary_site);
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate2_gurl),
      std::nullopt);
}

TEST_F(PrivacySandboxServiceTest, RwsPrefInit) {
  // Check that the init of the RWS pref occurs correctly.
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));

  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  // If the UI is available, the user blocks 3PC, and the pref has not been
  // previously init, it should be.
  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  // Once the pref has been init, it should not be re-init, and updated user
  // cookie settings should not impact it.
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  // Blocking all cookies should also init the RWS pref to off.
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));
}

TEST_F(PrivacySandboxServiceTest, UsesConfiguredRelatedWebsiteSets) {
  // Set up state for the RWS UI: block 3PC and enable the RWS pref.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Simulate that the Global RWS are ready with the following
  // set:
  // { primary: "https://youtube-primary.test",
  // associatedSites: ["https://youtube.com"]
  // }
  net::SchemefulSite youtube_primary_site(GURL("https://youtube-primary.test"));
  GURL youtube_gurl("https://youtube.com");
  net::SchemefulSite youtube_site(youtube_gurl);

  mock_first_party_sets_handler().SetGlobalSets(
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {youtube_primary_site,
               {net::FirstPartySetEntry(youtube_primary_site,
                                        net::SiteType::kPrimary)}},
              {youtube_site,
               {net::FirstPartySetEntry(youtube_primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {}));

  // Simulate that https://google.de is moved into a new RWS for this profile.
  mock_first_party_sets_handler().SetContextConfig(
      net::FirstPartySetsContextConfig::Create(
          {{net::SchemefulSite(GURL("https://google.de")),
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                net::SchemefulSite(GURL("https://new-primary.test")),
                net::SiteType::kAssociated))}})
          .value());

  first_party_sets_policy_service()->InitForTesting();

  EXPECT_EQ(privacy_sandbox_service()->GetRelatedWebsiteSetOwner(youtube_gurl),
            youtube_primary_site);
  EXPECT_FALSE(privacy_sandbox_service()->IsPartOfManagedRelatedWebsiteSet(
      net::SchemefulSite(GURL("https://googlesource.com"))));
  EXPECT_TRUE(privacy_sandbox_service()->IsPartOfManagedRelatedWebsiteSet(
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

TEST_F(PrivacySandboxServiceTest, LogPrivacySandboxState_APIs) {
  // Each test for the APIs are scoped below to ensure we start with a clean
  // HistogramTester as each call to `LogPrivacySandboxState` emits
  // histograms for all APIs.

  // Topics
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, true);
    privacy_sandbox_service()->LogPrivacySandboxState();

    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Topics.Enabled",
                                       static_cast<int>(true),
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.Topics.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneEnabled,
        /*expected_count=*/1);

    prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    privacy_sandbox_service()->LogPrivacySandboxState();

    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Topics.Enabled",
                                       static_cast<int>(false),
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.Topics.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneDisabled,
        /*expected_count=*/1);
  }

  // Fledge
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, true);
    privacy_sandbox_service()->LogPrivacySandboxState();

    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Fledge.Enabled",
                                       static_cast<int>(true),
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.Fledge.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneEnabled,
        /*expected_count=*/1);

    prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
    privacy_sandbox_service()->LogPrivacySandboxState();

    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Fledge.Enabled",
                                       static_cast<int>(false),
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.Fledge.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneDisabled,
        /*expected_count=*/1);
  }

  // Ad measurement
  {
    base::HistogramTester histogram_tester;
    prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
    privacy_sandbox_service()->LogPrivacySandboxState();

    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.Enabled", static_cast<int>(true),
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneEnabled,
        /*expected_count=*/1);
    prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, false);
    privacy_sandbox_service()->LogPrivacySandboxState();

    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.Enabled",
        static_cast<int>(false),
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneDisabled,
        /*expected_count=*/1);
  }
}

// Test class to verify that non-regular profiles (guest and incognito) emit
// only client-level histograms for privacy sandbox startup metrics.
class LogPrivacySandboxStateNonRegularProfilesTest
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<
          std::tuple<std::string,
                     std::string,
                     bool,
                     profile_metrics::BrowserProfileType>> {};

TEST_P(LogPrivacySandboxStateNonRegularProfilesTest, APIs) {
  auto [feature_name, feature_pref, is_enabled, profile_type] = GetParam();

  base::HistogramTester histogram_tester;

  profile_metrics::SetBrowserProfileType(profile(), profile_type);
  prefs()->SetBoolean(feature_pref, is_enabled);
  privacy_sandbox_service()->LogPrivacySandboxState();
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();

  // Check that no profile level histograms are emitted.
  EXPECT_THAT(
      histograms,
      testing::Not(testing::AnyOf(base::StrCat(
          {"Settings.PrivacySandbox.", feature_name, ".EnabledForProfile"}))));

  histogram_tester.ExpectBucketCount(
      base::StrCat({"Settings.PrivacySandbox.", feature_name, ".Enabled"}),
      static_cast<int>(is_enabled),
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    LogPrivacySandboxStateNonRegularProfilesTests,
    LogPrivacySandboxStateNonRegularProfilesTest,
    ::testing::Values(
        std::make_tuple("Topics",
                        prefs::kPrivacySandboxM1TopicsEnabled,
                        true,
                        profile_metrics::BrowserProfileType::kGuest),
        std::make_tuple("Fledge",
                        prefs::kPrivacySandboxM1FledgeEnabled,
                        true,
                        profile_metrics::BrowserProfileType::kGuest),
        std::make_tuple("AdMeasurement",
                        prefs::kPrivacySandboxM1AdMeasurementEnabled,
                        true,
                        profile_metrics::BrowserProfileType::kGuest),
        std::make_tuple("Topics",
                        prefs::kPrivacySandboxM1TopicsEnabled,
                        true,
                        profile_metrics::BrowserProfileType::kIncognito),
        std::make_tuple("Fledge",
                        prefs::kPrivacySandboxM1FledgeEnabled,
                        true,
                        profile_metrics::BrowserProfileType::kIncognito),
        std::make_tuple("AdMeasurement",
                        prefs::kPrivacySandboxM1AdMeasurementEnabled,
                        true,
                        profile_metrics::BrowserProfileType::kIncognito),
        std::make_tuple("Topics",
                        prefs::kPrivacySandboxM1TopicsEnabled,
                        false,
                        profile_metrics::BrowserProfileType::kGuest),
        std::make_tuple("Fledge",
                        prefs::kPrivacySandboxM1FledgeEnabled,
                        false,
                        profile_metrics::BrowserProfileType::kGuest),
        std::make_tuple("AdMeasurement",
                        prefs::kPrivacySandboxM1AdMeasurementEnabled,
                        false,
                        profile_metrics::BrowserProfileType::kGuest),
        std::make_tuple("Topics",
                        prefs::kPrivacySandboxM1TopicsEnabled,
                        false,
                        profile_metrics::BrowserProfileType::kIncognito),
        std::make_tuple("Fledge",
                        prefs::kPrivacySandboxM1FledgeEnabled,
                        false,
                        profile_metrics::BrowserProfileType::kIncognito),
        std::make_tuple("AdMeasurement",
                        prefs::kPrivacySandboxM1AdMeasurementEnabled,
                        false,
                        profile_metrics::BrowserProfileType::kIncognito)));

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

TEST_F(PrivacySandboxServiceTest, DisablePrivacySandboxTopicsPolicy) {
  base::HistogramTester histogram_tester;
  // Disable the Topics api via policy and check topics is not allowed.
  RunTestCase(TestState{{kM1TopicsDisabledByPolicy, true}}, TestInput{},
              TestOutput{{kIsTopicsAllowed, false}});
}

TEST_F(PrivacySandboxServiceTest, DisablePrivacySandboxFledgePolicy) {
  base::HistogramTester histogram_tester;
  // Disable the Fledge api via policy and check fledge is not allowed.
  RunTestCase(TestState{{kM1FledgeDisabledByPolicy, true}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kFledgeAuctionPartyOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsFledgeJoinAllowed, false}});
}

TEST_F(PrivacySandboxServiceTest, DisablePrivacySandboxAdMeasurementPolicy) {
  base::HistogramTester histogram_tester;
  // Disable the ad measurement api via policy and check the api is not allowed.
  RunTestCase(TestState{{kM1AdMesaurementDisabledByPolicy, true}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAdMeasurementReportingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsAttributionReportingAllowed, false}});
}

class PrivacySandboxNoticeFrameworkResultCallbackUnitTest
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<bool> {};

TEST_P(PrivacySandboxNoticeFrameworkResultCallbackUnitTest,
       UpdateTopicsApiResult_UpdatesCorrectly) {
  privacy_sandbox_service()->UpdateTopicsApiResult(GetParam());
  EXPECT_EQ(GetParam(),
            prefs()->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled));
}

TEST_P(PrivacySandboxNoticeFrameworkResultCallbackUnitTest,
       UpdateProtectedAudienceApiResult_UpdatesCorrectly) {
  privacy_sandbox_service()->UpdateProtectedAudienceApiResult(GetParam());
  EXPECT_EQ(GetParam(),
            prefs()->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled));
}

TEST_P(PrivacySandboxNoticeFrameworkResultCallbackUnitTest,
       UpdateMeasurementApiResult_UpdatesCorrectly) {
  privacy_sandbox_service()->UpdateMeasurementApiResult(GetParam());
  EXPECT_EQ(GetParam(),
            prefs()->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled));
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxNoticeFrameworkResultCallbackUnitTest,
                         PrivacySandboxNoticeFrameworkResultCallbackUnitTest,
                         testing::Bool());

class PrivacySandboxNoticeFrameworkEligibilityTest
    : public PrivacySandboxServiceTest {};

TEST_F(PrivacySandboxNoticeFrameworkEligibilityTest, EligibilityCallbacks) {
  // TODO(crbug.com/408017260): These are currently placeholders. Update tests
  // when real eligibility logic is implemented.
  EXPECT_EQ(privacy_sandbox_service()->GetTopicsApiEligibility(),
            EligibilityLevel::kNotEligible);
  EXPECT_EQ(privacy_sandbox_service()->GetProtectedAudienceApiEligibility(),
            EligibilityLevel::kNotEligible);
  EXPECT_EQ(privacy_sandbox_service()->GetAdMeasurementApiEligibility(),
            EligibilityLevel::kNotEligible);
}
