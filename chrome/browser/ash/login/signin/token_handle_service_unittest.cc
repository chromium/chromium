// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_service.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/types/optional_util.h"
#include "chrome/browser/ash/login/signin/token_handle_store_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kEmail[] = "user@example.com";
// Same account as `kEmail`, as displayed rather than canonicalized.
constexpr char kDisplayEmail[] = "User@example.com";
constexpr char kGaiaId[] = "fake-gaia-id";

constexpr char kTokenHandleStatusPref[] = "TokenHandleStatus";
constexpr std::string_view kTokenHandleStatusStale = "stale";

}  // namespace

class TokenHandleServiceTest : public testing::Test {
 public:
  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<FakeChromeUserManager>());
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(
        kEmail, IdentityTestEnvironmentProfileAdaptor::
                    GetIdentityTestEnvironmentFactories());
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id_, /*is_affiliated=*/false, user_manager::UserType::kRegular,
        profile_);
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    UserDataAuthClient::InitializeFake();
    token_handle_store_factory_ = std::make_unique<TokenHandleStoreFactory>(
        TestingBrowserProcess::GetGlobal()->local_state());
  }

  void TearDown() override {
    if (service_) {
      // `TokenHandleService::Shutdown()` is private; call it through the
      // `KeyedService` interface, as the keyed service framework does.
      static_cast<KeyedService*>(service_.get())->Shutdown();
      service_.reset();
    }
    token_handle_store_factory_.reset();
    UserDataAuthClient::Shutdown();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
    fake_user_manager_.Reset();
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  // Uses the display email so that only the Gaia ID matches `account_id_`.
  void MakePrimaryAccountAvailable() {
    identity_test_env()->MakeAccountAvailable(
        identity_test_env()
            ->CreateAccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithGaiaId(account_id_.GetGaiaId())
            .Build(kDisplayEmail));
  }

  void CreateService() {
    service_ = std::make_unique<TokenHandleService>(
        profile_, identity_test_env()->identity_manager(),
        TokenHandleStoreFactory::Get()->GetTokenHandleStore());
  }

  std::optional<std::string> GetTokenHandleStatus() {
    return base::OptionalFromPtr(
        user_manager::KnownUser(
            TestingBrowserProcess::GetGlobal()->local_state())
            .FindStringPath(account_id_, kTokenHandleStatusPref));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId(kEmail, GaiaId(kGaiaId));
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<TokenHandleStoreFactory> token_handle_store_factory_;
  std::unique_ptr<TokenHandleService> service_;
};

TEST_F(TokenHandleServiceTest, RefreshTokenUpdateRefetchesTokenHandle) {
  MakePrimaryAccountAvailable();
  CreateService();
  EXPECT_NE(GetTokenHandleStatus(), kTokenHandleStatusStale);

  base::test::TestFuture<void> access_token_requested;
  identity_test_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.GetCallback());
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  EXPECT_EQ(GetTokenHandleStatus(), kTokenHandleStatusStale);
  EXPECT_TRUE(access_token_requested.Wait());
}

TEST_F(TokenHandleServiceTest,
       RefreshTokenUpdateForSecondaryAccountsIsIgnored) {
  MakePrimaryAccountAvailable();
  CreateService();

  identity_test_env()->MakeAccountAvailable("secondary@example.com");

  EXPECT_FALSE(GetTokenHandleStatus().has_value());
}

}  // namespace ash
