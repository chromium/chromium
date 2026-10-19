// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_availability.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/browser_delegate/keyed_service_provider/manta_service_provider_impl.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/startup_visibility.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::mahi_availability {
namespace {

constexpr char kTestEmail[] = "user@example.com";
constexpr char kTestGaiaId[] = "gaia_id_for_user";
constexpr std::string_view kAllowedCountryCode = "us";
constexpr std::string_view kDisallowedCountryCode = "cn";

PrefService* local_state() {
  return TestingBrowserProcess::GetGlobal()->local_state();
}

// Exercises CanUseMahiService() and IsMahiAvailable() through a real logged-in
// user session, so the account resolves to its profile and the MantaService is
// looked up via MantaServiceProvider (not a null-guarded shortcut).
class MahiAvailabilityTest : public testing::Test {
 protected:
  void SetUp() override {
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            local_state(),
            std::make_unique<test::ChromeUserSessionTestEnvironmentDelegate>(
                TestingBrowserProcess::GetGlobal()));
    // Installs the process-wide MantaServiceProvider that CanUseMahiService()
    // reads. Its Find() resolves the account to its profile and calls
    // MantaServiceFactory, so it returns the MantaService the test configures.
    manta_service_provider_ = std::make_unique<MantaServiceProviderImpl>();

    SetCountryCode(kAllowedCountryCode);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state(), &metrics_enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    test_variations_service_ =
        std::make_unique<variations::TestVariationsService>(
            local_state(), metrics_state_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        test_variations_service_.get());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    test_variations_service_.reset();
    metrics_state_manager_.reset();

    manta_service_provider_.reset();
    user_session_test_environment_.reset();
  }

  void SetCountryCode(std::string_view country_code) {
    local_state()->SetString(variations::prefs::kVariationsCountry,
                             country_code);
  }

  void SetDemoMode() { settings_helper_.InstallAttributes()->SetDemoMode(); }

  // Logs in a regular user and returns its (annotated) profile.
  Profile* LoginRegularUser(const AccountId& account_id) {
    user_manager::User* user =
        user_session_test_environment_->AddRegularUser(account_id);
    CHECK(user);
    user_session_test_environment_->LogIn(account_id);
    return Profile::FromBrowserContext(
        BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id));
  }

  // Makes `account_id` the primary account of `profile` and sets its manta
  // capability: true=supported, false=minor-restricted, nullopt=not yet loaded.
  void SetMantaCapability(Profile* profile,
                          const AccountId& account_id,
                          std::optional<bool> can_use_manta) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    AccountInfo account;
    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      account = identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
    } else {
      signin::AccountAvailabilityOptionsBuilder builder;
      builder.AsPrimary(signin::ConsentLevel::kSignin)
          .WithGaiaId(account_id.GetGaiaId());
      account = signin::MakeAccountAvailable(
          identity_manager, builder.Build(account_id.GetUserEmail()));
    }
    if (can_use_manta.has_value()) {
      AccountCapabilitiesTestMutator mutator(&account);
      mutator.set_can_use_manta_service(can_use_manta.value());
      signin::UpdateAccountInfoForAccount(identity_manager, account);
    }
  }

  Profile* LogInRegularUserWithMantaCapability(
      std::optional<bool> can_use_manta) {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestEmail, GaiaId(kTestGaiaId));
    Profile* profile = LoginRegularUser(account_id);
    CHECK(profile);
    SetMantaCapability(profile, account_id, can_use_manta);
    return profile;
  }

  Profile* LogInGuestUser() {
    user_manager::User* user = user_session_test_environment_->AddGuestUser();
    CHECK(user);
    user_session_test_environment_->LogIn(user->GetAccountId());
    Profile* profile = Profile::FromBrowserContext(
        BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            user->GetAccountId()));
    CHECK(profile);
    profile->AsTestingProfile()->SetGuestSession(true);
    return profile;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<MantaServiceProviderImpl> manta_service_provider_;
  metrics::TestEnabledStateProvider metrics_enabled_state_provider_{
      /*consent=*/false, /*enabled=*/false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> test_variations_service_;
};

TEST_F(MahiAvailabilityTest, MinorRestrictedUserCannotUseMahi) {
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      "user@example.com", GaiaId("gaia_id_for_user"));
  Profile* profile = LoginRegularUser(account_id);
  ASSERT_TRUE(profile);
  SetMantaCapability(profile, account_id, /*can_use_manta=*/false);

  const base::expected<bool, Error> result = CanUseMahiService();

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(MahiAvailabilityTest, UnknownMantaCapabilityReportsFeatureBitNotReady) {
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      "user@example.com", GaiaId("gaia_id_for_user"));
  Profile* profile = LoginRegularUser(account_id);
  ASSERT_TRUE(profile);
  SetMantaCapability(profile, account_id, /*can_use_manta=*/std::nullopt);

  const base::expected<bool, Error> result = CanUseMahiService();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::kMantaFeatureBitNotReady);
}

TEST_F(MahiAvailabilityTest, CanUseMahiService_ReturnsFalseWhenNotLoggedIn) {
  EXPECT_EQ(CanUseMahiService(), base::ok(false));
}

TEST_F(MahiAvailabilityTest, CanUseMahiService_ReturnsFalseForGuestUser) {
  LogInGuestUser();
  EXPECT_EQ(CanUseMahiService(), base::ok(false));
}

TEST_F(MahiAvailabilityTest,
       CanUseMahiService_ReturnsTrueForAdultInAllowedCountry) {
  LogInRegularUserWithMantaCapability(/*can_use_manta=*/true);
  EXPECT_EQ(CanUseMahiService(), base::ok(true));
}

TEST_F(MahiAvailabilityTest,
       CanUseMahiService_ReturnsFalseInDisallowedCountry) {
  LogInRegularUserWithMantaCapability(/*can_use_manta=*/true);
  SetCountryCode(kDisallowedCountryCode);
  EXPECT_EQ(CanUseMahiService(), base::ok(false));

  SetCountryCode("");
  EXPECT_EQ(CanUseMahiService(), base::ok(false));
}

TEST_F(MahiAvailabilityTest,
       CanUseMahiService_ReturnsTrueWithRestrictionsOverrideSwitch) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      chromeos::switches::kMahiRestrictionsOverride);

  // Even when not logged in and in a disallowed country, the override switch
  // forces `CanUseMahiService()` to return true.
  SetCountryCode(kDisallowedCountryCode);
  EXPECT_EQ(CanUseMahiService(), base::ok(true));
}

TEST_F(MahiAvailabilityTest,
       CanUseMahiService_DemoModeBypassesUserAndMinorChecks) {
  SetDemoMode();

  // In demo mode, `CanUseMahiService()` skips login/minor checks and evaluates
  // country restrictions directly.
  SetCountryCode(kAllowedCountryCode);
  EXPECT_EQ(CanUseMahiService(), base::ok(true));

  SetCountryCode(kDisallowedCountryCode);
  EXPECT_EQ(CanUseMahiService(), base::ok(false));
}

TEST_F(MahiAvailabilityTest, IsMahiAvailable_ReturnsFalseWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      chromeos::features::kFeatureManagementMahi);

  LogInRegularUserWithMantaCapability(/*can_use_manta=*/true);
  ASSERT_EQ(CanUseMahiService(), base::ok(true));
  EXPECT_EQ(IsMahiAvailable(), base::ok(false));
}

TEST_F(MahiAvailabilityTest,
       IsMahiAvailable_PropagatesCanUseMahiServiceWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      chromeos::features::kFeatureManagementMahi);

  const AccountId account_id =
      AccountId::FromUserEmailGaiaId(kTestEmail, GaiaId(kTestGaiaId));
  Profile* profile = LoginRegularUser(account_id);
  ASSERT_TRUE(profile);

  SetMantaCapability(profile, account_id, /*can_use_manta=*/std::nullopt);
  EXPECT_EQ(IsMahiAvailable(),
            base::unexpected(Error::kMantaFeatureBitNotReady));

  SetMantaCapability(profile, account_id, /*can_use_manta=*/false);
  EXPECT_EQ(IsMahiAvailable(), base::ok(false));

  SetMantaCapability(profile, account_id, /*can_use_manta=*/true);
  EXPECT_EQ(IsMahiAvailable(), base::ok(true));

  SetCountryCode(kDisallowedCountryCode);
  EXPECT_EQ(IsMahiAvailable(), base::ok(false));
}

}  // namespace
}  // namespace ash::mahi_availability
