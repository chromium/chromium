// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_ash_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "content/public/test/browser_test.h"

namespace ash::auth {

using extensions::api::quick_unlock_private::TokenInfo;

class AuthFactorConfigTestBase : public MixinBasedInProcessBrowserTest {
 public:
  explicit AuthFactorConfigTestBase(ash::AshAuthFactor password_type) {
    test::UserAuthConfig config;
    if (password_type == ash::AshAuthFactor::kGaiaPassword) {
      config.WithOnlinePassword(test::kGaiaPassword);
    } else {
      CHECK_EQ(password_type, ash::AshAuthFactor::kLocalPassword);
      config.WithLocalPassword(test::kLocalPassword);
    }

    logged_in_user_mixin_ = std::make_unique<LoggedInUserMixin>(
        &mixin_host_, /*test_base=*/this, embedded_test_server(),
        LoggedInUserMixin::LogInType::kConsumer,
        /*include_initial_user=*/true,
        /*account_id=*/std::nullopt, config);
    cryptohome_ = &logged_in_user_mixin_->GetCryptohomeMixin();

    cryptohome_->set_enable_auth_check(true);
    cryptohome_->MarkUserAsExisting(logged_in_user_mixin_->GetAccountId());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    logged_in_user_mixin_->LogInUser();
  }

  // Create a new auth token. Returns nullopt if something went wrong, probably
  // because the provided password is incorrect.
  std::optional<std::string> MakeAuthToken(const std::string password) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    CHECK(profile);
    extensions::QuickUnlockPrivateGetAuthTokenHelper token_helper(profile,
                                                                  password);
    base::test::TestFuture<std::optional<TokenInfo>,
                           std::optional<AuthenticationError>>
        result;
    token_helper.Run(result.GetCallback());
    if (result.Get<0>().has_value()) {
      return result.Get<0>()->token;
    }

    CHECK(result.Get<1>().has_value());
    return std::nullopt;
  }

 protected:
  std::unique_ptr<LoggedInUserMixin> logged_in_user_mixin_;
  raw_ptr<CryptohomeMixin> cryptohome_{nullptr};
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AuthFactorConfigTestWithLocalPassword : public AuthFactorConfigTestBase {
 public:
  AuthFactorConfigTestWithLocalPassword()
      : AuthFactorConfigTestBase(ash::AshAuthFactor::kLocalPassword) {}
};

// Checks that PasswordFactorEditor::UpdateOrSetLocalPassword can be used to set
// a new password. This test is mostly here to make sure that the test fixture
// works as intended.
IN_PROC_BROWSER_TEST_F(AuthFactorConfigTestWithLocalPassword,
                       UpdateLocalPasswordSuccess) {
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());
  mojom::PasswordFactorEditor& password_editor =
      GetPasswordFactorEditor(quick_unlock::QuickUnlockFactory::GetDelegate(),
                              g_browser_process->local_state());

  base::test::TestFuture<mojom::ConfigureResult> result;
  password_editor.UpdateOrSetLocalPassword(*auth_token, test::kNewPassword,
                                           result.GetCallback());

  ASSERT_EQ(result.Get(), mojom::ConfigureResult::kSuccess);
  // Since MakeAuthToken authenticates using the provided password, this will
  // check that the new password works:
  auth_token = MakeAuthToken(test::kNewPassword);
  ASSERT_TRUE(auth_token.has_value());
}

// Checks that PasswordFactorEditor::UpdateOrSetLocalPassword rejects
// insufficiently complex passwords.
IN_PROC_BROWSER_TEST_F(AuthFactorConfigTestWithLocalPassword,
                       UpdateLocalPasswordComplexityFailure) {
  static const std::string kBadPassword = "asdfas∆";

  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());
  mojom::PasswordFactorEditor& password_editor =
      GetPasswordFactorEditor(quick_unlock::QuickUnlockFactory::GetDelegate(),
                              g_browser_process->local_state());

  base::test::TestFuture<mojom::ConfigureResult> result;
  password_editor.UpdateOrSetLocalPassword(*auth_token, kBadPassword,
                                           result.GetCallback());

  ASSERT_EQ(result.Get(), mojom::ConfigureResult::kFatalError);
  // Since MakeAuthToken authenticates using the provided password, this will
  // check that the bad password really hasn't been set:
  auth_token = MakeAuthToken(kBadPassword);
  ASSERT_TRUE(!auth_token.has_value());
}

class AuthFactorConfigTestWithGaiaPassword : public AuthFactorConfigTestBase {
 public:
  AuthFactorConfigTestWithGaiaPassword()
      : AuthFactorConfigTestBase(ash::AshAuthFactor::kGaiaPassword) {}
};

class ChangeGaiaPasswordFactorTest
    : public AuthFactorConfigTestWithGaiaPassword {
 public:
  ChangeGaiaPasswordFactorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kChangePasswordFactorSetup);
  }
};

IN_PROC_BROWSER_TEST_F(ChangeGaiaPasswordFactorTest,
                       UpdateToLocalPasswordSuccess) {
  std::optional<std::string> auth_token = MakeAuthToken(test::kGaiaPassword);
  ASSERT_TRUE(auth_token.has_value());
  mojom::PasswordFactorEditor& password_editor =
      GetPasswordFactorEditor(quick_unlock::QuickUnlockFactory::GetDelegate(),
                              g_browser_process->local_state());

  base::test::TestFuture<mojom::ConfigureResult> result;
  password_editor.UpdateOrSetLocalPassword(*auth_token, test::kLocalPassword,
                                           result.GetCallback());

  ASSERT_EQ(result.Get(), mojom::ConfigureResult::kSuccess);
  // Since MakeAuthToken authenticates using the provided password, this will
  // check that the new password works:
  auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());
}

}  // namespace ash::auth
