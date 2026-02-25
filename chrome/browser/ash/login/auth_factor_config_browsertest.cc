// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash::auth {

using extensions::api::quick_unlock_private::TokenInfo;

namespace {

// A complex password of length 12 containing lowercase and uppercase
// characters, digits, and symbols.
constexpr std::string kComplexPassword = "abcDEF123+-%";

// A simple password of length 14. It passes the original check which was only
// checking that the length was at least 8.
constexpr std::string kSimplePassword = "simplepassword";

// An invalid token.
constexpr std::string kInvalidToken = "invalid_token";

}  // namespace

class AuthFactorConfigTestBase : public MixinBasedInProcessBrowserTest {
 public:
  explicit AuthFactorConfigTestBase(ash::AshAuthFactor password_type) {
    test::UserAuthConfig config;
    if (password_type == ash::AshAuthFactor::kGaiaPassword) {
      config.WithOnlinePassword(test::kGaiaPassword);
    } else if (password_type == ash::AshAuthFactor::kLocalPassword) {
      config.WithLocalPassword(test::kLocalPassword);
    } else {
      CHECK_EQ(password_type, ash::AshAuthFactor::kCryptohomePin);
      config.WithCryptohomePin(test::kAuthPin, test::kPinStubSalt);
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
  std::optional<std::string> MakeAuthToken(std::string password) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    CHECK(profile);
    extensions::QuickUnlockPrivateGetAuthTokenHelper token_helper(
        profile, std::move(password));
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

  mojom::PasswordFactorEditor& password_editor() {
    return ash::auth::GetPasswordFactorEditor(
        quick_unlock::QuickUnlockFactory::GetDelegate(),
        g_browser_process->local_state());
  }

  mojom::ConfigureResult UpdateOrSetLocalPassword(const std::string& auth_token,
                                                  const std::string& password) {
    base::test::TestFuture<mojom::ConfigureResult> future;
    password_editor().UpdateOrSetLocalPassword(auth_token, password,
                                               future.GetCallback());
    return future.Get();
  }

  mojom::ConfigureResult SetLocalPassword(const std::string& auth_token,
                                          const std::string& password) {
    base::test::TestFuture<mojom::ConfigureResult> future;
    password_editor().SetLocalPassword(auth_token, password,
                                       future.GetCallback());
    return future.Get();
  }

  mojom::ConfigureResult UpdateOrSetOnlinePassword(
      const std::string& auth_token,
      const std::string& password) {
    base::test::TestFuture<mojom::ConfigureResult> future;
    password_editor().UpdateOrSetOnlinePassword(auth_token, password,
                                                future.GetCallback());
    return future.Get();
  }

  mojom::ConfigureResult SetOnlinePassword(const std::string& auth_token,
                                           const std::string& password) {
    base::test::TestFuture<mojom::ConfigureResult> future;
    password_editor().SetOnlinePassword(auth_token, password,
                                        future.GetCallback());
    return future.Get();
  }

  mojom::PasswordFactorEditor::CheckLocalPasswordComplexityResult
  CheckLocalPasswordComplexity(const std::string& auth_token,
                               const std::string& password) {
    base::test::TestFuture<
        mojom::PasswordFactorEditor::CheckLocalPasswordComplexityResult>
        future;
    password_editor().CheckLocalPasswordComplexity(auth_token, password,
                                                   future.GetCallback());
    return future.Take();
  }

  void SetComplexityPolicy(ash::LocalAuthFactorsComplexity complexity) {
    GetProfile()->GetPrefs()->SetInteger(
        ash::prefs::kLocalAuthFactorsComplexity, static_cast<int>(complexity));
  }

 protected:
  std::unique_ptr<LoggedInUserMixin> logged_in_user_mixin_;
  raw_ptr<CryptohomeMixin> cryptohome_{nullptr};
};

class AuthFactorConfigTestWithCryptohomePin : public AuthFactorConfigTestBase {
 public:
  AuthFactorConfigTestWithCryptohomePin()
      : AuthFactorConfigTestBase(ash::AshAuthFactor::kCryptohomePin) {}
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

class AuthFactorConfigTestWithLocalPasswordWithManagedLocalPinAndPasswordEnabled
    : public AuthFactorConfigTestWithLocalPassword {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    AuthFactorConfigTestWithLocalPassword::SetUpInProcessBrowserTestFixture();
    // Initialize user policy.
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void DisableAllAllowedAuthFactorsPolicy() {
    policy::PolicyMap user_policy;
    base::Value allowed_auth_factors(base::Value::Type::LIST);

    user_policy.Set(policy::key::kAllowedLocalAuthFactors,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_CLOUD,
                    std::move(allowed_auth_factors), nullptr);
    provider_.UpdateChromePolicy(user_policy);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  base::test::ScopedFeatureList feature_list_{
      ash::features::kManagedLocalPinAndPassword};
};

// Checks that PasswordFactorEditor::UpdateOrSetPassword can be used to set
// an online password for a user having a local password, as that should be
// valid when the policy is disabled.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordWithManagedLocalPinAndPasswordEnabled,
    UpdateLocalPasswordToGaiaPassword) {
  DisableAllAllowedAuthFactorsPolicy();
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());
  mojom::PasswordFactorEditor& password_editor =
      GetPasswordFactorEditor(quick_unlock::QuickUnlockFactory::GetDelegate(),
                              g_browser_process->local_state());

  base::test::TestFuture<mojom::ConfigureResult> result;
  password_editor.UpdateOrSetOnlinePassword(*auth_token, test::kGaiaPassword,
                                            result.GetCallback());

  ASSERT_EQ(result.Get(), mojom::ConfigureResult::kSuccess);
  // Since MakeAuthToken authenticates using the provided password, this will
  // check that the new password works:
  auth_token = MakeAuthToken(test::kGaiaPassword);
  ASSERT_TRUE(auth_token.has_value());
}

class AuthFactorConfigTestWithGaiaPassword : public AuthFactorConfigTestBase {
 public:
  AuthFactorConfigTestWithGaiaPassword()
      : AuthFactorConfigTestBase(ash::AshAuthFactor::kGaiaPassword) {}
};

IN_PROC_BROWSER_TEST_F(AuthFactorConfigTestWithGaiaPassword,
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

// Checks that PasswordFactorEditor::UpdateOrSetOnlinePassword does not
// reject insufficiently complex password, as it is on the online IdP
// to enforce appropriate complexity.
IN_PROC_BROWSER_TEST_F(AuthFactorConfigTestWithGaiaPassword,
                       UpdateOnlinePasswordNoComplexityCheck) {
  static const std::string kShortPassword = "short";

  std::optional<std::string> auth_token = MakeAuthToken(test::kGaiaPassword);
  ASSERT_TRUE(auth_token.has_value());
  mojom::PasswordFactorEditor& password_editor =
      GetPasswordFactorEditor(quick_unlock::QuickUnlockFactory::GetDelegate(),
                              g_browser_process->local_state());

  base::test::TestFuture<mojom::ConfigureResult> result;
  password_editor.UpdateOrSetOnlinePassword(*auth_token, kShortPassword,
                                            result.GetCallback());

  ASSERT_NE(result.Get(), mojom::ConfigureResult::kFatalError);

  // Since MakeAuthToken authenticates using the provided password, this will
  // check that the new password works:
  auth_token = MakeAuthToken(kShortPassword);
  ASSERT_TRUE(auth_token.has_value());
}

// -----------------------------------------------------------------------------
// --------------------- LocalAuthFactorsComplexity tests ----------------------
// -----------------------------------------------------------------------------

class AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity
    : public AuthFactorConfigTestWithLocalPassword {
 public:
  AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity() {
    feature_list_.InitAndEnableFeature(
        ash::features::kLocalFactorsPasswordComplexity);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AuthFactorConfigTestWithLocalPasswordWithoutLocalAuthFactorsComplexity
    : public AuthFactorConfigTestWithLocalPassword {
 public:
  AuthFactorConfigTestWithLocalPasswordWithoutLocalAuthFactorsComplexity() {
    feature_list_.InitAndDisableFeature(
        ash::features::kLocalFactorsPasswordComplexity);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AuthFactorConfigTestWithCryptohomePinAndLocalAuthFactorsComplexity
    : public AuthFactorConfigTestWithCryptohomePin {
 public:
  AuthFactorConfigTestWithCryptohomePinAndLocalAuthFactorsComplexity() {
    feature_list_.InitAndEnableFeature(
        ash::features::kLocalFactorsPasswordComplexity);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AuthFactorConfigTestWithGaiaPasswordAndLocalAuthFactorsComplexity
    : public AuthFactorConfigTestWithGaiaPassword {
 public:
  AuthFactorConfigTestWithGaiaPasswordAndLocalAuthFactorsComplexity() {
    feature_list_.InitAndEnableFeature(
        ash::features::kLocalFactorsPasswordComplexity);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that CheckLocalPasswordComplexity returns kInvalidTokenError when
// provided with an invalid auth token.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity,
    CheckLocalPasswordComplexity_InvalidToken) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  auto result = CheckLocalPasswordComplexity(kInvalidToken, kComplexPassword);

  EXPECT_EQ(result.error(), mojom::ConfigureResult::kInvalidTokenError);
}

// Checks that CheckLocalPasswordComplexity returns kOk for a valid password.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity,
    CheckLocalPasswordComplexity_Success) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  auto result = CheckLocalPasswordComplexity(*auth_token, kComplexPassword);

  EXPECT_EQ(result, mojom::PasswordComplexity::kOk);
}

// Checks that CheckLocalPasswordComplexity returns kTooShort for a password
// that doesn't pass the complexity requirements.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity,
    CheckLocalPasswordComplexity_TooShort) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  auto result = CheckLocalPasswordComplexity(*auth_token, kSimplePassword);

  EXPECT_EQ(result, mojom::PasswordComplexity::kTooShort);
}

// Checks that CheckLocalPasswordComplexity returns kOk for a password
// that doesn't pass the complexity requirements, but passes the old check when
// the policy is not enabled.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity,
    CheckLocalPasswordComplexity_TooShort_NoPolicy) {
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  auto result = CheckLocalPasswordComplexity(*auth_token, kSimplePassword);

  EXPECT_EQ(result, mojom::PasswordComplexity::kOk);
}

// Checks that CheckLocalPasswordComplexity returns kOk when provided with an
// invalid auth token when the new feature isn't enabled.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordWithoutLocalAuthFactorsComplexity,
    CheckLocalPasswordComplexity_InvalidToken_Success) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  auto result = CheckLocalPasswordComplexity(kInvalidToken, kComplexPassword);

  EXPECT_EQ(result, mojom::PasswordComplexity::kOk);
}

// Checks that CheckLocalPasswordComplexity returns kOk for a password
// that doesn't pass the complexity requirements, but passes the old check when
// the new feature isn't enabled.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordWithoutLocalAuthFactorsComplexity,
    CheckLocalPasswordComplexity_TooShort_Success) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  auto result = CheckLocalPasswordComplexity(*auth_token, kSimplePassword);

  EXPECT_EQ(result, mojom::PasswordComplexity::kOk);
}

// Checks that UpdateOrSetLocalPassword rejects a password failing the policy
// check, and accepts a password passing the policy check.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity,
    UpdateOrSetLocalPassword_EnforcesPolicy) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  // 1. Verify rejection of a simple password.
  EXPECT_EQ(UpdateOrSetLocalPassword(*auth_token, kSimplePassword),
            mojom::ConfigureResult::kFatalError);

  // 2. Verify acceptance of a complex password.
  EXPECT_EQ(UpdateOrSetLocalPassword(*auth_token, kComplexPassword),
            mojom::ConfigureResult::kSuccess);
}

// Checks that UpdateOrSetLocalPassword works as before when the policy is not
// enabled.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithLocalPasswordAndLocalAuthFactorsComplexity,
    UpdateOrSetLocalPassword_NoPolicy) {
  std::optional<std::string> auth_token = MakeAuthToken(test::kLocalPassword);
  ASSERT_TRUE(auth_token.has_value());

  EXPECT_EQ(UpdateOrSetLocalPassword(*auth_token, kSimplePassword),
            mojom::ConfigureResult::kSuccess);
}

// Checks that SetLocalPassword rejects a password failing the policy check, and
// accepts a password passing the policy check.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithCryptohomePinAndLocalAuthFactorsComplexity,
    SetLocalPassword_EnforcesPolicy) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kAuthPin);
  ASSERT_TRUE(auth_token.has_value());

  // 1. Verify SetLocalPassword rejects simple password.
  EXPECT_EQ(SetLocalPassword(*auth_token, kSimplePassword),
            mojom::ConfigureResult::kFatalError);

  // 2. Verify SetLocalPassword accepts complex password.
  EXPECT_EQ(SetLocalPassword(*auth_token, kComplexPassword),
            mojom::ConfigureResult::kSuccess);
}

// Checks that SetLocalPassword works as before when the policy is not enabled.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithCryptohomePinAndLocalAuthFactorsComplexity,
    SetLocalPassword_NoPolicy) {
  std::optional<std::string> auth_token = MakeAuthToken(test::kAuthPin);
  ASSERT_TRUE(auth_token.has_value());

  EXPECT_EQ(SetLocalPassword(*auth_token, kSimplePassword),
            mojom::ConfigureResult::kSuccess);
}

// Checks that UpdateOrSetOnlinePassword ignores the complexity policy for an
// online password.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithGaiaPasswordAndLocalAuthFactorsComplexity,
    UpdateOrSetOnlinePassword_NoComplexityCheck) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kGaiaPassword);
  ASSERT_TRUE(auth_token.has_value());

  EXPECT_EQ(UpdateOrSetOnlinePassword(*auth_token, kSimplePassword),
            mojom::ConfigureResult::kSuccess);
}

// Checks that SetOnlinePassword ignores the complexity policy for an online
// password.
IN_PROC_BROWSER_TEST_F(
    AuthFactorConfigTestWithCryptohomePinAndLocalAuthFactorsComplexity,
    SetOnlinePassword_NoComplexityCheck) {
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  std::optional<std::string> auth_token = MakeAuthToken(test::kAuthPin);
  ASSERT_TRUE(auth_token.has_value());

  EXPECT_EQ(SetOnlinePassword(*auth_token, kSimplePassword),
            mojom::ConfigureResult::kSuccess);
}

// -----------------------------------------------------------------------------
// -------------------- /LocalAuthFactorsComplexity tests ----------------------
// -----------------------------------------------------------------------------

}  // namespace ash::auth
