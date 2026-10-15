// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_availability.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/browser_delegate/keyed_service_provider/manta_service_provider_impl.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::mahi_availability {
namespace {

// Exercises CanUseMahiService() through a real logged-in user session, so the
// account resolves to its profile and the MantaService is looked up via
// MantaServiceProvider (not a null-guarded shortcut).
class MahiAvailabilityTest : public testing::Test {
 protected:
  void SetUp() override {
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state(),
            std::make_unique<test::ChromeUserSessionTestEnvironmentDelegate>(
                TestingBrowserProcess::GetGlobal()));
    // Installs the process-wide MantaServiceProvider that CanUseMahiService()
    // reads. Its Find() resolves the account to its profile and calls
    // MantaServiceFactory, so it returns the MantaService the test configures.
    manta_service_provider_ = std::make_unique<MantaServiceProviderImpl>();
  }

  void TearDown() override {
    manta_service_provider_.reset();
    user_session_test_environment_.reset();
  }

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
    signin::AccountAvailabilityOptionsBuilder builder;
    builder.AsPrimary(signin::ConsentLevel::kSignin)
        .WithGaiaId(account_id.GetGaiaId());
    AccountInfo account = signin::MakeAccountAvailable(
        identity_manager, builder.Build(account_id.GetUserEmail()));
    if (can_use_manta.has_value()) {
      AccountCapabilitiesTestMutator mutator(&account);
      mutator.set_can_use_manta_service(can_use_manta.value());
      signin::UpdateAccountInfoForAccount(identity_manager, account);
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<MantaServiceProviderImpl> manta_service_provider_;
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

}  // namespace
}  // namespace ash::mahi_availability
