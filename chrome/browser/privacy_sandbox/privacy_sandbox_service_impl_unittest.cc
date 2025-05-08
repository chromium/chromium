// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <tuple>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/profile_bucket_metrics.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
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
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/privacy_sandbox/mock_privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
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
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/login_state/scoped_test_public_session_login_state.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace {
using ::browsing_topics::Topic;
using ::privacy_sandbox::CanonicalTopic;

using PromptAction = ::PrivacySandboxService::PromptAction;
using PromptSuppressedReason = ::PrivacySandboxService::PromptSuppressedReason;
using PromptType = ::PrivacySandboxService::PromptType;
using SurfaceType = ::PrivacySandboxService::SurfaceType;
using NoticeSurfaceType = ::privacy_sandbox::SurfaceType;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::ValuesIn;

using Notice = privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using NoticeEvent = privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using PrimaryAccountUserGroups =
    ::PrivacySandboxService::PrimaryAccountUserGroups;
using FakeNoticePromptSuppressionReason =
    ::PrivacySandboxService::FakeNoticePromptSuppressionReason;

using enum privacy_sandbox_test_util::StateKey;
using enum privacy_sandbox_test_util::InputKey;
using enum privacy_sandbox_test_util::OutputKey;
using enum PrivacySandboxService::PromptAction;

using privacy_sandbox_test_util::InputKey;
using privacy_sandbox_test_util::OutputKey;
using privacy_sandbox_test_util::StateKey;

using privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using privacy_sandbox_test_util::MultipleInputKeys;
using privacy_sandbox_test_util::MultipleOutputKeys;
using privacy_sandbox_test_util::MultipleStateKeys;
using privacy_sandbox_test_util::SiteDataExceptions;
using privacy_sandbox_test_util::TestCase;
using privacy_sandbox_test_util::TestInput;
using privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::TestState;

const char kFirstPartySetsStateHistogram[] = "Settings.FirstPartySets.State";
const char kDefaultProfileUsername[] = "user@gmail.com";
const char kTestEmail[] = "test@test.com";

const base::Version kRelatedWebsiteSetsVersion("1.2.3");

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

  virtual void InitializeFeaturesBeforeStart() {}

  void CreateDefaultProfile() {
    default_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
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
    privacy_sandbox_service_.reset();

    auto mock_delegate = CreateMockDelegate();
    mock_delegate_ = mock_delegate.get();
    mock_privacy_sandbox_countries_ =
        std::make_unique<MockPrivacySandboxCountries>();

    privacy_sandbox_settings_ =
        std::make_unique<privacy_sandbox::PrivacySandboxSettingsImpl>(
            std::move(mock_delegate), host_content_settings_map(),
            cookie_settings(), tracking_protection_settings(), prefs());
    privacy_sandbox_service_ = std::make_unique<PrivacySandboxServiceImpl>(
        profile(), privacy_sandbox_settings(), tracking_protection_settings(),
        cookie_settings(), profile()->GetPrefs(), test_interest_group_manager(),
        GetProfileType(), browsing_data_remover(), host_content_settings_map(),
        mock_browsing_topics_service(), first_party_sets_policy_service(),
        mock_privacy_sandbox_countries());
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

  PrefService* local_state() { return local_state_.Get(); }
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
    return first_party_sets_policy_service_.get();
  }

  MockPrivacySandboxCountries* mock_privacy_sandbox_countries() {
    return mock_privacy_sandbox_countries_.get();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  content::BrowserTaskEnvironment* browser_task_environment() {
    return &browser_task_environment_;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;

  // In production, ProfileManager is created much earlier than Profile
  // creation. Some of the tests using this fixture needs local_state,
  // so instead of let TestingProfileManager generate it, we instantiate
  // it independently.
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
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
        privacy_sandbox::kPrivacySandboxAdTopicsContentParity);
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

TEST_F(PrivacySandboxServiceTest, PromptActionsUMAActions) {
  base::UserActionTester user_action_tester;

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName, "true"}});
  privacy_sandbox_service()->PromptActionOccurred(kNoticeShown,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(
      user_action_tester.GetActionCount("Settings.PrivacySandbox.Notice.Shown"),
      1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeOpenSettings,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.OpenedSettings"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeAcknowledge,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.Acknowledged"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeDismiss,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.Dismissed"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeClosedNoInteraction,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.ClosedNoInteraction"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeLearnMore,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.LearnMore"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeMoreInfoOpened,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.LearnMoreExpanded"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeMoreInfoClosed,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.LearnMoreClosed"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kConsentMoreButtonClicked,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.MoreButtonClicked"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kNoticeMoreButtonClicked,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.MoreButtonClicked"),
            1);

  // Site Suggested Ads & Ads Measurement more info dropdown prompt actions part
  // of Ads API UX Enhancements.
  privacy_sandbox_service()->PromptActionOccurred(
      kNoticeSiteSuggestedAdsMoreInfoOpened, SurfaceType::kDesktop);
  EXPECT_EQ(
      user_action_tester.GetActionCount(
          "Settings.PrivacySandbox.Notice.SiteSuggestedAdsLearnMoreExpanded"),
      1);

  privacy_sandbox_service()->PromptActionOccurred(
      kNoticeSiteSuggestedAdsMoreInfoClosed, SurfaceType::kDesktop);
  EXPECT_EQ(
      user_action_tester.GetActionCount(
          "Settings.PrivacySandbox.Notice.SiteSuggestedAdsLearnMoreClosed"),
      1);

  privacy_sandbox_service()->PromptActionOccurred(
      kNoticeAdsMeasurementMoreInfoOpened, SurfaceType::kDesktop);
  EXPECT_EQ(
      user_action_tester.GetActionCount(
          "Settings.PrivacySandbox.Notice.AdsMeasurementLearnMoreExpanded"),
      1);

  privacy_sandbox_service()->PromptActionOccurred(
      kNoticeAdsMeasurementMoreInfoClosed, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Notice.AdsMeasurementLearnMoreClosed"),
            1);

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName, "true"}});

  privacy_sandbox_service()->PromptActionOccurred(kConsentShown,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.Shown"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kConsentAccepted,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.Accepted"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kConsentDeclined,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.Declined"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kConsentMoreInfoOpened,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.LearnMoreExpanded"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kPrivacyPolicyLinkClicked,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.PrivacyPolicyLinkClicked"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kConsentMoreInfoClosed,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.LearnMoreClosed"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kConsentClosedNoDecision,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.Consent.ClosedNoInteraction"),
            1);

  feature_list()->Reset();
  feature_list()->InitAndEnableFeatureWithParameters(
      privacy_sandbox::kPrivacySandboxSettings4,
      {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName, "true"},
       {privacy_sandbox::kPrivacySandboxSettings4RestrictedNoticeName,
        "true"}});

  privacy_sandbox_service()->PromptActionOccurred(kRestrictedNoticeOpenSettings,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.OpenedSettings"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kRestrictedNoticeAcknowledge,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.Acknowledged"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(kRestrictedNoticeShown,
                                                  SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.Shown"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      kRestrictedNoticeClosedNoInteraction, SurfaceType::kDesktop);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Settings.PrivacySandbox.RestrictedNotice.ClosedNoInteraction"),
            1);

  privacy_sandbox_service()->PromptActionOccurred(
      kRestrictedNoticeMoreButtonClicked, SurfaceType::kDesktop);
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

using PrivacySandboxDarkLaunchMetrics = PrivacySandboxServiceTest;

TEST_F(PrivacySandboxDarkLaunchMetrics,
       IdentityManagerHistogramSkippedForNonRegularProfile) {
  base::HistogramTester histogram_tester;
  SetProfileType(profile_metrics::BrowserProfileType::kGuest);
  CreateService();
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.DarkLaunch.IdentityManagerSuccess", 0);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       IdentityManagerHistogramEmittedForRegularProfile) {
  base::HistogramTester histogram_tester;
  SetProfileType(profile_metrics::BrowserProfileType::kRegular);
  CreateService();
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.DarkLaunch.IdentityManagerSuccess", 1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       NoDarkLaunchStartupMetricsIfNotRegularProfile) {
  SetProfileType(profile_metrics::BrowserProfileType::kIncognito);
  CreateService();
  base::HistogramTester histogram_tester;
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.DarkLaunch.Profile_1.PrimaryAccountOnStartup", 0);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       NoDarkLaunchStartupMetricsOnSubsequentGetRequiredPromptCalls) {
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);
  base::HistogramTester histogram_tester;
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);
  histogram_tester.ExpectTotalCount(
      "PrivacySandbox.DarkLaunch.Profile_1.PrimaryAccountOnStartup", 0);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PrivacySandboxDarkLaunchMetrics, PrimaryAccountSignedOutOnStartup) {
  // First GetRequiredPromptType call triggers startup histograms
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);

  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();

  EXPECT_THAT(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime),
              base::Time());
  EXPECT_THAT(
      histograms,
      testing::Not(testing::AnyOf(
          "PrivacySandbox.DarkLaunch.Profile_1.ProfileSignInDuration")));
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.PrimaryAccountOnStartup",
      PrimaryAccountUserGroups::kSignedOut, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PrivacySandboxDarkLaunchMetrics,
       PrimaryAccountSignedInCapabilityUnknownOnStartup) {
  EnableSignIn();
  // First GetRequiredPromptType call triggers startup histograms
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);

  EXPECT_THAT(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime),
              testing::Not(base::Time()));
  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              testing::ContainsRegex(
                  "PrivacySandbox.DarkLaunch.Profile_1.ProfileSignInDuration"));
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.PrimaryAccountOnStartup",
      PrimaryAccountUserGroups::kSignedInCapabilityUnknown, 1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       PrimaryAccountSignedInCapabilityFalseOnStartup) {
  EnableSignInU18();
  // First GetRequiredPromptType call triggers startup histograms
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);

  EXPECT_THAT(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime),
              testing::Not(base::Time()));
  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              testing::ContainsRegex(
                  "PrivacySandbox.DarkLaunch.Profile_1.ProfileSignInDuration"));
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.PrimaryAccountOnStartup",
      PrimaryAccountUserGroups::kSignedInCapabilityFalse, 1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       PrimaryAccountSignedInCapabilityTrueOnStartup) {
  EnableSignInOver18();
  // First GetRequiredPromptType call triggers startup histograms
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);

  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              testing::ContainsRegex(
                  "PrivacySandbox.DarkLaunch.Profile_1.ProfileSignInDuration"));
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.PrimaryAccountOnStartup",
      PrimaryAccountUserGroups::kSignedInCapabilityTrue, 1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       PrimaryAccountAlreadySignedInOnStartup) {
  EnableSignInOver18();
  prefs()->SetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime,
                   base::Time());
  // Initial sign in
  privacy_sandbox_service()->GetRequiredPromptType(
      PrivacySandboxService::SurfaceType::kDesktop);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.UnknownProfileSignInDuration", true,
      1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics, OnPrimaryAccountChangedSignIn) {
  EnableSignIn();
  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              testing::ContainsRegex(
                  "PrivacySandbox.DarkLaunch.Profile_1.ProfileSignInDuration"));

  auto sign_in_time =
      prefs()->GetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime);
  EXPECT_THAT(sign_in_time, testing::Not(base::Time()));

#if !BUILDFLAG(IS_CHROMEOS)
  // Signing in again should not change the metrics.
  SignOut();
  EnableSignIn();
  EXPECT_THAT(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticeFirstSignInTime),
              sign_in_time);
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PrivacySandboxDarkLaunchMetrics,
       UserGroupTransitionsEmitMetricsSuccessfully) {
  // kNotSet -> kSignedOut
  // kSignedOut -> kSignedInCapabilityTrue
  EnableSignInOver18();
  // kSignedInCapabilityTrue -> kSignedOut -> kSignedInCapabilityFalse
  SignOut();
  EnableSignInU18();

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.UserGroups",
      PrimaryAccountUserGroups::kSignedOut, 2);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.UserGroups",
      PrimaryAccountUserGroups::kSignedInCapabilityTrue, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.UserGroups",
      PrimaryAccountUserGroups::kSignedInCapabilityFalse, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PrivacySandboxDarkLaunchMetrics, FakeNoticeShown) {
  EnableSignInOver18();
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);
  base::Time notice_shown = base::Time::Now();

  // Advancing time by an arbitrary amount to imitate function being called
  // multiple times at different points. The set pref should not be overridden.
  base::TimeDelta delay = base::Seconds(15);
  browser_task_environment()->FastForwardBy(delay);

  // The prompt should only track as shown the first time.
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptShown", true, 1);
  EXPECT_EQ(
      prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTimeSync),
      notice_shown);

  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref.PromptShown", true, 1);
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTime),
            notice_shown);
}

TEST_F(PrivacySandboxDarkLaunchMetrics, FakeNoticePromptShownSince) {
  EnableSignInOver18();
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  base::TimeDelta delay = base::Days(15);
  browser_task_environment()->FastForwardBy(delay);
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kNoticeShownBefore, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptShownSince", 15, 1);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kNoticeShownBefore, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref.PromptShownSince", 15,
      1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       FakeNoticeSuppressedDueToManagedProfile) {
  prefs()->SetManagedPref(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kManagedDevice, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kManagedDevice, 1);
  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptShown")));
  EXPECT_EQ(
      prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTimeSync),
      base::Time());

  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref.PromptShown")));
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTime),
            base::Time());
}

TEST_F(PrivacySandboxDarkLaunchMetrics, FakeNoticeSuppressedDueTo3PCBlocked) {
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);
  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptShown")));
  EXPECT_EQ(
      prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTimeSync),
      base::Time());

  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref.PromptShown")));
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTime),
            base::Time());
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       FakeNoticeSuppressedDueToAccountCapabilityFalse) {
  EnableSignInU18();
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kCapabilityFalse, 1);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kCapabilityFalse, 1);
  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptShown")));
  EXPECT_EQ(
      prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTimeSync),
      base::Time());

  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref.PromptShown")));
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTime),
            base::Time());
}

// Histograms should only record once if `GetRequiredPromptType` is called
// multiple times with the same eligibility.
TEST_F(PrivacySandboxDarkLaunchMetrics,
       FakeNoticeMultipleSuppressionReasonsRecordOnlyOnce) {
  // Signing in with capability true should give suppression 0.
  EnableSignInOver18();
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  // Next time the function is called suppression should trigger already shown &
  // we add 3pc blocking.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  // Call the function again with no eligibility changes.
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  // Relevant histograms should only be emitted once.
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kNoticeShownBefore, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kNoticeShownBefore, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);

  // Combined bit would be 1001.
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReasonsCombined",
      9, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReasonsCombined",
      9, 1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics,
       FakeNoticeMultipleSuppressionReasonsRecordsAgainOnceEligibilityChanges) {
  // Signing in with capability true should give suppression 0.
  EnableSignInOver18();
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  // Next time the function is called suppression should trigger already shown
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  // Call the function again with 3pc blocked.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  // Allow 3pc some histograms should be emitted again.
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kNoticeShownBefore, 3);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kNoticeShownBefore, 3);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);

  // Emit twice for already shown: 1000 (initial call and after allowing 3pc).
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReasonsCombined",
      8, 2);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReasonsCombined",
      8, 2);

  // Emit once for 3pc, already shown: 1001
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReasonsCombined",
      9, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReasonsCombined",
      9, 1);
}

TEST_F(PrivacySandboxDarkLaunchMetrics, FakeNoticeMultipleSuppressionReasons) {
  // U18 and 3PC Blocking
  EnableSignInU18();
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop);

  // Both histograms should be emitted.
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kCapabilityFalse, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::kCapabilityFalse, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReason",
      FakeNoticePromptSuppressionReason::k3PC_Blocked, 1);

  // Combined bit would be 0011.
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref."
      "PromptSuppressionReasonsCombined",
      3, 1);
  histogram_tester()->ExpectBucketCount(
      "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref."
      "PromptSuppressionReasonsCombined",
      3, 1);

  // Prompt shown metrics should not be recorded.
  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.SyncedPref.PromptShown")));
  EXPECT_EQ(
      prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTimeSync),
      base::Time());

  EXPECT_THAT(
      histograms,
      testing::Not(testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.NonSyncedPref.PromptShown")));
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxFakeNoticePromptShownTime),
            base::Time());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PrivacySandboxDarkLaunchMetrics, OnPrimaryAccountChangedSignOut) {
  EnableSignIn();
  SignOut();

  EXPECT_THAT(
      prefs()->GetTime(prefs::kPrivacySandboxFakeNoticeFirstSignOutTime),
      testing::Not(base::Time()));

  const std::string histograms = histogram_tester()->GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      testing::ContainsRegex(
          "PrivacySandbox.DarkLaunch.Profile_1.ProfileSignOutDuration"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

using PrivacySandboxServiceDeathTest = PrivacySandboxServiceTest;

TEST_F(PrivacySandboxServiceDeathTest, TPSettingsNullExpectDeath) {
  ASSERT_DEATH(
      {
        PrivacySandboxServiceImpl(
            profile(), privacy_sandbox_settings(),
            /*tracking_protection_settings=*/nullptr, cookie_settings(),
            profile()->GetPrefs(), test_interest_group_manager(),
            GetProfileType(), browsing_data_remover(),
            host_content_settings_map(), mock_browsing_topics_service(),
            first_party_sets_policy_service(),
            mock_privacy_sandbox_countries());
      },
      "");
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
TEST_F(PrivacySandboxServiceTest,
       GetRelatedWebsiteSetOwner_SimulatedRwsData_DisabledWhen3pcAllowed) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global RWS with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets(
      kRelatedWebsiteSetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated)}},
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
  net::GlobalFirstPartySets global_sets(
      kRelatedWebsiteSetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated)}},
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
  net::GlobalFirstPartySets global_sets(
      kRelatedWebsiteSetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated)}},
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
  mock_first_party_sets_handler().SetGlobalSets(net::GlobalFirstPartySets(
      kRelatedWebsiteSetsVersion,
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary)}},
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated)}},
          {associate2_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated)}},
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

  mock_first_party_sets_handler().SetGlobalSets(net::GlobalFirstPartySets(
      kRelatedWebsiteSetsVersion,
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

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Explicitly) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";
  // TODO(crbug.com/385345006): Add support for multi profile testing.
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";

  prefs()->SetInteger(prefs::kPrivacySandboxM1PromptSuppressed,
                      static_cast<int>(PromptSuppressedReason::kRestricted));
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueToPrivacySandboxRestricted),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kRestrictedNoticeNotShownDueToNoticeShownToGuardian),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptSuppressed_Implicitly) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";

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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kPromptNotShownDueToManagedState),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_EEA) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";

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

  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
      static_cast<int>(PrivacySandboxServiceImpl::PromptStartupState::
                           kEEANoticePromptWaiting),
      /*expected_count=*/1);
}

TEST_F(PrivacySandboxServiceTest,
       RecordPrivacySandbox4StartupMetrics_PromptNotSuppressed_ROW) {
  base::HistogramTester histogram_tester;
  const std::string privacy_sandbox_prompt_startup_histogram =
      "Settings.PrivacySandbox.PromptStartupState";
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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

  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.Topics.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneEnabled,
        /*expected_count=*/1);

    prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();

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
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();

    histogram_tester.ExpectBucketCount("Settings.PrivacySandbox.Fledge.Enabled",
                                       static_cast<int>(true),
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.Fledge.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneEnabled,
        /*expected_count=*/1);

    prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();

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
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();

    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.Enabled", static_cast<int>(true),
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Settings.PrivacySandbox.AdMeasurement.EnabledForProfile",
        privacy_sandbox::ProfileEnabledState::kPSProfileOneEnabled,
        /*expected_count=*/1);
    prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, false);
    privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();

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
class PrivacySandbox4StartupMetricsNonRegularProfilesTest
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<
          std::tuple<std::string,
                     std::string,
                     bool,
                     profile_metrics::BrowserProfileType>> {};

TEST_P(PrivacySandbox4StartupMetricsNonRegularProfilesTest, APIs) {
  std::string feature_name = std::get<0>(GetParam());
  std::string feature_pref = std::get<1>(GetParam());
  bool is_enabled = std::get<2>(GetParam());
  profile_metrics::BrowserProfileType profile_type = std::get<3>(GetParam());

  base::HistogramTester histogram_tester;

  profile_metrics::SetBrowserProfileType(profile(), profile_type);
  prefs()->SetBoolean(feature_pref, is_enabled);
  privacy_sandbox_service()->RecordPrivacySandbox4StartupMetrics();
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
    PrivacySandbox4StartupMetricsNonRegularProfilesTests,
    PrivacySandbox4StartupMetricsNonRegularProfilesTest,
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

struct SurfaceMapping {
  PrivacySandboxService::SurfaceType input_surface;
  NoticeSurfaceType expected_notice_surface;
};

struct NoticeActionData {
  base::test::FeatureRefAndParams feature_flag;
  PromptAction action_occurred;
  Notice expected_notice;
  NoticeEvent expected_event;
};

class PrivacySandboxNoticeServiceInteractionTest
    : public PrivacySandboxServiceTest,
      public testing::WithParamInterface<
          std::tuple<SurfaceMapping, NoticeActionData>> {
 public:
  PrivacySandboxNoticeServiceInteractionTest() {
    const auto& core_data = std::get<1>(GetParam());
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{core_data.feature_flag,
                              {privacy_sandbox::
                                   kPsDualWritePrefsToNoticeStorage,
                               {}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    mock_notice_service_ = static_cast<
        privacy_sandbox::MockPrivacySandboxNoticeService*>(
        PrivacySandboxNoticeServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(
                    &privacy_sandbox::BuildMockPrivacySandboxNoticeService)));
    PrivacySandboxServiceTest::SetUp();
  }

  void TearDown() override {
    PrivacySandboxServiceTest::TearDown();
    mock_notice_service_ = nullptr;
  }

  privacy_sandbox::MockPrivacySandboxNoticeService* mock_notice_service() {
    return mock_notice_service_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<privacy_sandbox::MockPrivacySandboxNoticeService>
      mock_notice_service_ = nullptr;
};

TEST_P(PrivacySandboxNoticeServiceInteractionTest,
       VerifyNoticeServiceEventOccurred) {
  const auto& surface_mapping = std::get<0>(GetParam());
  const auto& core_data = std::get<1>(GetParam());

  EXPECT_CALL(*mock_notice_service(),
              EventOccurred(Pair(Eq(core_data.expected_notice),
                                 Eq(surface_mapping.expected_notice_surface)),
                            Eq(core_data.expected_event)))
      .Times(1);

  privacy_sandbox_service()->PromptActionOccurred(
      core_data.action_occurred, surface_mapping.input_surface);

  testing::Mock::VerifyAndClearExpectations(mock_notice_service());
}

const std::vector<SurfaceMapping> kSurfaceMappings = {
    {.input_surface = SurfaceType::kDesktop,
     .expected_notice_surface = NoticeSurfaceType::kDesktopNewTab},
    {.input_surface = SurfaceType::kBrApp,
     .expected_notice_surface = NoticeSurfaceType::kClankBrApp},
    {.input_surface = SurfaceType::kAGACCT,
     .expected_notice_surface = NoticeSurfaceType::kClankCustomTab},
};

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

const std::vector<NoticeActionData> kNoticeActionDataList = {
    // --- Shown Actions ---
    {.feature_flag = ConsentFeature(),
     .action_occurred = PromptAction::kConsentShown,
     .expected_notice = Notice::kTopicsConsentNotice,
     .expected_event = NoticeEvent::kShown},
    {.feature_flag = ConsentFeature(),
     .action_occurred = PromptAction::kNoticeShown,
     .expected_notice = Notice::kProtectedAudienceMeasurementNotice,
     .expected_event = NoticeEvent::kShown},
    {.feature_flag = NoticeFeature(),
     .action_occurred = PromptAction::kNoticeShown,
     .expected_notice = Notice::kThreeAdsApisNotice,
     .expected_event = NoticeEvent::kShown},
    {.feature_flag = RestrictedNoticeFeature(),
     .action_occurred = PromptAction::kRestrictedNoticeShown,
     .expected_notice = Notice::kMeasurementNotice,
     .expected_event = NoticeEvent::kShown},

    // --- Consent Actions ---
    {.feature_flag = ConsentFeature(),
     .action_occurred = PromptAction::kConsentAccepted,
     .expected_notice = Notice::kTopicsConsentNotice,
     .expected_event = NoticeEvent::kOptIn},
    {.feature_flag = ConsentFeature(),
     .action_occurred = PromptAction::kConsentDeclined,
     .expected_notice = Notice::kTopicsConsentNotice,
     .expected_event = NoticeEvent::kOptOut},

    // --- Ack Actions ---
    {.feature_flag = ConsentFeature(),
     .action_occurred = PromptAction::kNoticeAcknowledge,
     .expected_notice = Notice::kProtectedAudienceMeasurementNotice,
     .expected_event = NoticeEvent::kAck},
    {.feature_flag = NoticeFeature(),
     .action_occurred = PromptAction::kNoticeAcknowledge,
     .expected_notice = Notice::kThreeAdsApisNotice,
     .expected_event = NoticeEvent::kAck},
    {.feature_flag = RestrictedNoticeFeature(),
     .action_occurred = PromptAction::kRestrictedNoticeAcknowledge,
     .expected_notice = Notice::kMeasurementNotice,
     .expected_event = NoticeEvent::kAck},

    // --- Settings Actions ---
    {.feature_flag = ConsentFeature(),
     .action_occurred = PromptAction::kNoticeOpenSettings,
     .expected_notice = Notice::kProtectedAudienceMeasurementNotice,
     .expected_event = NoticeEvent::kSettings},
    {.feature_flag = NoticeFeature(),
     .action_occurred = PromptAction::kNoticeOpenSettings,
     .expected_notice = Notice::kThreeAdsApisNotice,
     .expected_event = NoticeEvent::kSettings},
    {.feature_flag = RestrictedNoticeFeature(),
     .action_occurred = PromptAction::kRestrictedNoticeOpenSettings,
     .expected_notice = Notice::kMeasurementNotice,
     .expected_event = NoticeEvent::kSettings},
};

INSTANTIATE_TEST_SUITE_P(PrivacySandboxNoticeServiceInteractionTest,
                         PrivacySandboxNoticeServiceInteractionTest,
                         Combine(ValuesIn(kSurfaceMappings),
                                 ValuesIn(kNoticeActionDataList)));

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
  RunTestCase(TestState{{kM1AdMeasurementEnabledUserPrefValue, false},
                        {kM1RestrictedNoticePreviouslyAcknowledged, false}},
              TestInput{{kPromptAction,
                         static_cast<int>(kRestrictedNoticeAcknowledge)}},
              TestOutput{{kM1AdMeasurementEnabled, true},
                         {kM1RestrictedNoticeAcknowledged, true}});

  // Open settings action should update the prefs accordingly.
  RunTestCase(TestState{{kM1AdMeasurementEnabledUserPrefValue, false},
                        {kM1RestrictedNoticePreviouslyAcknowledged, false}},
              TestInput{{kPromptAction,
                         static_cast<int>(kRestrictedNoticeOpenSettings)}},
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

TEST_F(PrivacySandboxServiceM1DelayCreation,
       ActivateAllowPromptForBlocked3PCookiesWhenPrefSet) {
  // Setup
  base::FieldTrial* trial(
      base::FieldTrialList::CreateFieldTrial("AllowPromptFor3PCStudy", "A"));

  auto local_feature_list = std::make_unique<base::FeatureList>();
  local_feature_list->RegisterFieldTrialOverride(
      privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  feature_list()->InitWithFeatureList(std::move(local_feature_list));

  prefs()->SetBoolean(prefs::kPrivacySandboxAllowNoticeFor3PCBlockedTrial,
                      true);

  // Action
  CreateService();

  // Verification
  auto* field_trial = base::FeatureList::GetFieldTrial(
      privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies);

  ASSERT_TRUE(field_trial);
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(field_trial->trial_name()));
}

TEST_F(PrivacySandboxServiceM1DelayCreation,
       DoNotActivateAllowPromptForBlocked3PCookiesWhenPrefNotSet) {
  // Setup
  base::FieldTrial* trial(
      base::FieldTrialList::CreateFieldTrial("AllowPromptFor3PCStudy", "A"));

  auto local_feature_list = std::make_unique<base::FeatureList>();
  local_feature_list->RegisterFieldTrialOverride(
      privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies.name,
      base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  feature_list()->InitWithFeatureList(std::move(local_feature_list));

  prefs()->SetBoolean(prefs::kPrivacySandboxAllowNoticeFor3PCBlockedTrial,
                      false);

  // Action
  CreateService();

  // Verification
  auto* field_trial = base::FeatureList::GetFieldTrial(
      privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies);

  ASSERT_TRUE(field_trial);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(field_trial->trial_name()));
}

TEST_F(
    PrivacySandboxServiceM1DelayCreation,
    ThirdPartyCookieBlockedSuppressReasonClearedWhenAllowPromptFeatureEnabled) {
  feature_list()->InitAndEnableFeature(
      privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PromptSuppressedReason::kThirdPartyCookiesBlocked));

  CreateService();

  EXPECT_EQ(prefs()->GetValue(prefs::kPrivacySandboxM1PromptSuppressed),
            static_cast<int>(PromptSuppressedReason::kNone));
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxAllowNoticeFor3PCBlockedTrial));
}

TEST_F(
    PrivacySandboxServiceM1DelayCreation,
    ThirdPartyCookieBlockedSuppressReasonClearedWhenAllowPromptFeatureDisabled) {
  feature_list()->InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies);

  prefs()->SetInteger(
      prefs::kPrivacySandboxM1PromptSuppressed,
      static_cast<int>(PromptSuppressedReason::kThirdPartyCookiesBlocked));

  CreateService();

  EXPECT_EQ(
      prefs()->GetValue(prefs::kPrivacySandboxM1PromptSuppressed),
      static_cast<int>(PromptSuppressedReason::kThirdPartyCookiesBlocked));
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxAllowNoticeFor3PCBlockedTrial));
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
    feature_list()->InitWithFeaturesAndParameters(
        {{privacy_sandbox::kPrivacySandboxSettings4,
          {{privacy_sandbox::kPrivacySandboxSettings4ConsentRequiredName,
            "true"},
           {privacy_sandbox::kPrivacySandboxSettings4NoticeRequiredName,
            "false"}}}},
        {privacy_sandbox::kPrivacySandboxAllowPromptForBlocked3PCookies});
  }
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrivacySandboxServiceM1PromptTest, DeviceLocalAccountUser) {
  privacy_sandbox_service()->ForceChromeBuildForTests(true);
  user_manager::ScopedUserManager user_manager(
      std::make_unique<user_manager::FakeUserManager>(local_state()));

  // No prompt should be shown for a public session account.
  ash::ScopedTestPublicSessionLoginState login_state;
  // TODO(crbug.com/361794340): Ensure the promptType is correct across
  // different surfaceTypes.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRequiredPromptType(SurfaceType::kDesktop),
      PromptType::kNone);

  // A prompt should be shown for a regular user.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      ash::LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
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
      TestInput{{kPromptAction, static_cast<int>(kConsentAccepted)}},
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
      TestInput{{kPromptAction, static_cast<int>(kConsentDeclined)}},
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
              TestInput{{kPromptAction, static_cast<int>(kNoticeAcknowledge)}},
              TestOutput{{kM1EEANoticeAcknowledged, true},
                         {kM1FledgeEnabled, true},
                         {kM1AdMeasurementEnabled, true}});
  RunTestCase(
      TestState{{kM1ConsentDecisionPreviouslyMade, true},
                {kM1EEANoticePreviouslyAcknowledged, false}},
      TestInput{{kPromptAction, static_cast<int>(kNoticeOpenSettings)}},
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
              TestInput{{kPromptAction, static_cast<int>(kNoticeAcknowledge)}},
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
              TestInput{{kPromptAction, static_cast<int>(kNoticeAcknowledge)}},
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
              TestInput{{kPromptAction, static_cast<int>(kNoticeOpenSettings)}},
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
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";

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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
      static_cast<int>(
          PrivacySandboxServiceImpl::PromptStartupState::
              kRestrictedNoticeNotShownDueToFullNoticeAcknowledged),
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
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";

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
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram_profile_level,
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
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram_profile_level,
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
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";

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
  histogram_tester.ExpectBucketCount(
      privacy_sandbox_prompt_startup_histogram_profile_level,
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
  const std::string privacy_sandbox_prompt_startup_histogram_profile_level =
      "Settings.PrivacySandbox.Profile_1.PromptStartupState";

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
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram_profile_level,
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
    histogram_tester.ExpectBucketCount(
        privacy_sandbox_prompt_startup_histogram_profile_level,
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
