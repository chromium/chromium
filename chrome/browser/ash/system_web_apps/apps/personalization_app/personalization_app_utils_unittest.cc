// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/generative_ai_country_restrictions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/startup_visibility.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::personalization_app {

namespace {

constexpr std::string_view kAllowedCountryCode = "us";
constexpr std::string_view kBlockedCountryCode = "zz";

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId(
        "user@example.com",
        GaiaId::Literal("gaia_id_for_user"));

PrefService* local_state() {
  return TestingBrowserProcess::GetGlobal()->local_state();
}

class PersonalizationAppUtilsTest : public testing::Test {
 public:
  PersonalizationAppUtilsTest() = default;
  PersonalizationAppUtilsTest(const PersonalizationAppUtilsTest&) = delete;
  PersonalizationAppUtilsTest& operator=(const PersonalizationAppUtilsTest&) =
      delete;
  ~PersonalizationAppUtilsTest() override = default;

  TestingProfile* SetUpUserAndProfile(const AccountId& account_id,
                                      user_manager::UserType user_type) {
    AddAndLoginUser(account_id, user_type);
    ash::ScopedAccountIdAnnotator annotator(profile_manager_->profile_manager(),
                                            account_id);
    TestingProfile* profile =
        user_type == user_manager::UserType::kGuest
            ? profile_manager_->CreateGuestProfile()
            : profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    user_manager::UserManager::Get()->OnUserProfileCreated(account_id,
                                                           profile->GetPrefs());
    return profile;
  }

  void SetUpIdentityAndCapabilities(TestingProfile* profile,
                                    const AccountId& account_id,
                                    std::optional<bool> can_use_manta) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    signin::AccountAvailabilityOptionsBuilder builder;
    builder.AsPrimary(signin::ConsentLevel::kSignin)
        .WithGaiaId(account_id.GetGaiaId());
    AccountInfo primary_account = signin::MakeAccountAvailable(
        identity_manager, builder.Build(account_id.GetUserEmail()));

    if (can_use_manta.has_value()) {
      AccountCapabilitiesTestMutator mutator(&primary_account);
      mutator.set_can_use_manta_service(can_use_manta.value());
      signin::UpdateAccountInfoForAccount(identity_manager, primary_account);
    }
  }

  void SetUp() override {
    testing::Test::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {features::kSeaPenDemoMode, features::kFeatureManagementSeaPen},
        {features::kGrowthCampaignsInDemoMode, features::kGrowthFramework});

    user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(local_state());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    // Ensures ProfileHelper / BrowserContextHelper singleton is initialized.
    TestingBrowserProcess::GetGlobal()->platform_part()->profile_helper();

    browser_controller_ = std::make_unique<ash::BrowserControllerImpl>();

    CHECK(ash::IsGenerativeAiAllowedForCountry(kAllowedCountryCode));
    SetCountryCode(kAllowedCountryCode);

    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state(), &metrics_enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    test_variations_service_ =
        std::make_unique<variations::TestVariationsService>(
            local_state(), metrics_state_manager_.get());
    test_variations_service_->OverrideStoredPermanentCountry(
        std::string(kAllowedCountryCode));
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        test_variations_service_.get());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    test_variations_service_.reset();
    metrics_state_manager_.reset();
    user_session_manager_.reset();
    profile_manager_.reset();
    browser_controller_.reset();
    testing::Test::TearDown();
  }

  void SetCountryCode(std::string_view country_code) {
    local_state()->SetString(variations::prefs::kVariationsCountry,
                             country_code);
  }

 private:
  void AddAndLoginUser(const AccountId& account_id,
                       const user_manager::UserType user_type) {
    user_manager::User* user = nullptr;
    switch (user_type) {
      case user_manager::UserType::kRegular:
        user = user_session_manager_->AddRegularUser(account_id);
        break;
      case user_manager::UserType::kGuest:
        EXPECT_EQ(account_id, user_manager::GuestAccountId());
        user = user_session_manager_->AddGuestUser();
        break;
      case user_manager::UserType::kChild:
        user = user_session_manager_->AddChildUser(account_id);
        break;
      case user_manager::UserType::kPublicAccount:
        user = user_session_manager_->AddPublicAccountUser(
            account_id.GetUserEmail());
        break;
      default:
        []() { GTEST_FAIL() << "Unsupported user type"; }();
        return;
    }
    ASSERT_TRUE(user);
    user_session_manager_->LogIn(user->GetAccountId());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::BrowserControllerImpl> browser_controller_;
  std::unique_ptr<ash::test::TestUserSessionManager> user_session_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  metrics::TestEnabledStateProvider metrics_enabled_state_provider_{
      /*consent=*/false, /*enabled=*/false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> test_variations_service_;
};

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenGuest) {
  auto* guest_profile = SetUpUserAndProfile(user_manager::GuestAccountId(),
                                            user_manager::UserType::kGuest);
  ASSERT_FALSE(IsAllowedToInstallSeaPen(guest_profile));
  ASSERT_FALSE(IsEligibleForSeaPen(guest_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenChild) {
  auto* child_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kChild);
  child_profile->SetIsSupervisedProfile();
  ASSERT_FALSE(IsAllowedToInstallSeaPen(child_profile));
  ASSERT_FALSE(IsEligibleForSeaPen(child_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenNoProfile) {
  ASSERT_FALSE(IsAllowedToInstallSeaPen(nullptr));
  ASSERT_FALSE(IsEligibleForSeaPen(nullptr));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenGoogler) {
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      "user@google.com", GaiaId("gaia_id_for_googler"));
  auto* googler_profile =
      SetUpUserAndProfile(account_id, user_manager::UserType::kRegular);
  googler_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  ASSERT_TRUE(IsAllowedToInstallSeaPen(googler_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenManaged) {
  auto* managed_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);
  managed_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  ASSERT_TRUE(IsAllowedToInstallSeaPen(managed_profile));
  ASSERT_FALSE(IsEligibleForSeaPen(managed_profile));
}

TEST_F(PersonalizationAppUtilsTest,
       IsEligibleForSeaPenManagedSeaPenEnterpriseEnabledCapabilityIsTrue) {
  auto* managed_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  managed_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  SetUpIdentityAndCapabilities(managed_profile, kTestAccountId, true);

  ASSERT_TRUE(IsAllowedToInstallSeaPen(managed_profile));
  ASSERT_TRUE(IsEligibleForSeaPen(managed_profile));
}

TEST_F(PersonalizationAppUtilsTest,
       IsEligibleForSeaPenManagedSeaPenEnterpriseEnabledCapabilityIsFalse) {
  auto* managed_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  managed_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  SetUpIdentityAndCapabilities(managed_profile, kTestAccountId, false);

  ASSERT_TRUE(IsAllowedToInstallSeaPen(managed_profile));
  ASSERT_FALSE(IsEligibleForSeaPen(managed_profile));
}

TEST_F(PersonalizationAppUtilsTest,
       IsEligibleForSeaPenManagedSeaPenEnterpriseEnabledCapabilityIsUnknown) {
  auto* managed_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  managed_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  SetUpIdentityAndCapabilities(managed_profile, kTestAccountId, std::nullopt);

  ASSERT_TRUE(IsAllowedToInstallSeaPen(managed_profile));
  ASSERT_FALSE(IsEligibleForSeaPen(managed_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenRegular) {
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);
  ASSERT_TRUE(IsAllowedToInstallSeaPen(regular_profile));
  ASSERT_TRUE(IsEligibleForSeaPen(regular_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPen_BlockedCountryCode) {
  ASSERT_FALSE(ash::IsGenerativeAiAllowedForCountry(kBlockedCountryCode));
  SetCountryCode(kBlockedCountryCode);

  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);
  ASSERT_TRUE(IsAllowedToInstallSeaPen(regular_profile));
  ASSERT_FALSE(IsEligibleForSeaPen(regular_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenPublicAccount) {
  const std::string account_id_str = "public-account";
  const std::string email = policy::GenerateDeviceLocalAccountUserId(
      account_id_str, policy::DeviceLocalAccountType::kPublicSession);
  AccountId account_id = AccountId::FromUserEmail(email);
  auto* managed_profile =
      SetUpUserAndProfile(account_id, user_manager::UserType::kPublicAccount);
  managed_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  ASSERT_FALSE(IsAllowedToInstallSeaPen(managed_profile));
  ASSERT_FALSE(IsEligibleForSeaPen(managed_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenPublicAccountDemoMode) {
  const std::string account_id_str = "demo-public-account";
  const std::string email = policy::GenerateDeviceLocalAccountUserId(
      account_id_str, policy::DeviceLocalAccountType::kPublicSession);
  AccountId account_id = AccountId::FromUserEmail(email);
  auto* managed_profile =
      SetUpUserAndProfile(account_id, user_manager::UserType::kPublicAccount);
  managed_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  // Force device into demo mode.
  ASSERT_FALSE(ash::demo_mode::IsDeviceInDemoMode());
  managed_profile->ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetDemoMode();
  ASSERT_TRUE(ash::demo_mode::IsDeviceInDemoMode());

  // Force demo mode session to start.
  ASSERT_FALSE(::ash::DemoSession::Get());
  auto demo_mode_test_helper = std::make_unique<::ash::DemoModeTestHelper>();
  demo_mode_test_helper->InitializeSession();
  ASSERT_TRUE(::ash::DemoSession::Get());

  ASSERT_TRUE(IsAllowedToInstallSeaPen(managed_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenTextInput_UnknownAge) {
  base::test::ScopedFeatureList features(features::kSeaPenTextInput);
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  SetUpIdentityAndCapabilities(regular_profile, kTestAccountId, std::nullopt);

  ASSERT_FALSE(IsEligibleForSeaPenTextInput(regular_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenTextInput_MinorUser) {
  base::test::ScopedFeatureList features(features::kSeaPenTextInput);
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  SetUpIdentityAndCapabilities(regular_profile, kTestAccountId, false);

  ASSERT_FALSE(IsEligibleForSeaPenTextInput(regular_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenTextInput_AdultUser) {
  base::test::ScopedFeatureList features(features::kSeaPenTextInput);
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  SetUpIdentityAndCapabilities(regular_profile, kTestAccountId, true);

  ASSERT_TRUE(IsEligibleForSeaPenTextInput(regular_profile));
}

TEST_F(PersonalizationAppUtilsTest, IsEligibleForSeaPenTextInputEnglishUsers) {
  base::test::ScopedFeatureList features(features::kSeaPenTextInput);
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  SetUpIdentityAndCapabilities(regular_profile, kTestAccountId, true);

  // Set application locale.
  g_browser_process->SetApplicationLocale("en-GB");

  ASSERT_TRUE(IsSystemInSupportedLanguage());
  ASSERT_TRUE(IsEligibleForSeaPenTextInput(regular_profile));
}

TEST_F(PersonalizationAppUtilsTest,
       IsEligibleForSeaPenTextInputNonEnglishUsers_TranslationDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kSeaPenTextInput},
                            {features::kSeaPenTextInputTranslation});
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  SetUpIdentityAndCapabilities(regular_profile, kTestAccountId, true);

  // Set application locale.
  g_browser_process->SetApplicationLocale("de");

  ASSERT_FALSE(IsSystemInSupportedLanguage());
  ASSERT_FALSE(IsEligibleForSeaPenTextInput(regular_profile));
}

TEST_F(PersonalizationAppUtilsTest,
       IsEligibleForSeaPenTextInputSupportedLanguageUsers_TranslationEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kSeaPenTextInput, features::kSeaPenTextInputTranslation}, {});
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  SetUpIdentityAndCapabilities(regular_profile, kTestAccountId, true);

  // Set application locale.
  g_browser_process->SetApplicationLocale("de");

  ASSERT_TRUE(IsSystemInSupportedLanguage());
  ASSERT_TRUE(IsEligibleForSeaPenTextInput(regular_profile));
}

TEST_F(
    PersonalizationAppUtilsTest,
    IsEligibleForSeaPenTextInputUnsupportedLanguageUsers_TranslationEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kSeaPenTextInput, features::kSeaPenTextInputTranslation}, {});
  auto* regular_profile =
      SetUpUserAndProfile(kTestAccountId, user_manager::UserType::kRegular);

  SetUpIdentityAndCapabilities(regular_profile, kTestAccountId, true);

  // Set application locale.
  g_browser_process->SetApplicationLocale("no");

  ASSERT_FALSE(IsSystemInSupportedLanguage());
  ASSERT_FALSE(IsEligibleForSeaPenTextInput(regular_profile));
}

}  // namespace

}  // namespace ash::personalization_app
