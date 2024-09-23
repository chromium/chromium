// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/platform_keys/key_permissions/fake_user_private_token_kpm_service.h"
#include "chrome/browser/ash/platform_keys/key_permissions/mock_key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/attestation/keystore.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;

namespace extensions {
namespace {

const char kUserEmail[] = "test@google.com";

void FakeRunCheckNotRegister(::attestation::VerifiedAccessFlow flow_type,
                             Profile* profile,
                             ash::attestation::TpmChallengeKeyCallback callback,
                             const std::string& challenge,
                             bool register_key,
                             ::attestation::KeyType key_crypto_type,
                             const std::string& key_name_for_spkac,
                             const std::optional<std::string>& signals) {
  EXPECT_FALSE(register_key);
  std::move(callback).Run(
      ash::attestation::TpmChallengeKeyResult::MakeChallengeResponse(
          "response"));
}

class EPKChallengeKeyTestBase : public BrowserWithTestWindowTest {
 protected:
  EPKChallengeKeyTestBase() : extension_(ExtensionBuilder("Test").Build()) {
    stub_install_attributes_.SetCloudManaged("google.com", "device_id");
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    prefs_ = browser()->profile()->GetPrefs();
    SetAuthenticatedUser();

    // UserPrivateTokenKeyPermissionsManagerService and the underlying
    // KeyPermissionsManager are not actually used by *ChallengeKey* classes,
    // but they are created as a part of KeystoreService, so just fake them out.
    // It is ok to pass an unretained pointer because the factory should only be
    // used during the tests' lifetime.
    ash::platform_keys::UserPrivateTokenKeyPermissionsManagerServiceFactory::
        GetInstance()
            ->SetTestingFactory(
                browser()->profile(),
                base::BindRepeating(&EPKChallengeKeyTestBase::
                                        CreateKeyPermissionsManagerService,
                                    base::Unretained(this)));
  }

  void SetMockTpmChallenger() {
    auto mock_tpm_challenge_key =
        std::make_unique<NiceMock<ash::attestation::MockTpmChallengeKey>>();
    // Will be used with EXPECT_CALL.
    mock_tpm_challenge_key_ = mock_tpm_challenge_key.get();
    mock_tpm_challenge_key->EnableFake();
    // transfer ownership inside factory
    ash::attestation::TpmChallengeKeyFactory::SetForTesting(
        std::move(mock_tpm_challenge_key));
  }

  void SetMockTpmChallengerBadBase64Error() {
    auto mock_tpm_challenge_key =
        std::make_unique<NiceMock<ash::attestation::MockTpmChallengeKey>>();
    // Error text is "Challenge is not base64 encoded."
    mock_tpm_challenge_key->EnableFakeError(
        ash::attestation::TpmChallengeKeyResultCode::kChallengeBadBase64Error);
    // transfer ownership inside factory
    ash::attestation::TpmChallengeKeyFactory::SetForTesting(
        std::move(mock_tpm_challenge_key));
  }

  // This will be called by BrowserWithTestWindowTest::SetUp();
  std::string GetDefaultProfileName() override { return kUserEmail; }

  void LogIn(const std::string& email) override {
    const AccountId account_id = AccountId::FromUserEmail(email);
    user_manager()->AddUserWithAffiliation(account_id,
                                           /*is_affiliated=*/true);
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }

  std::unique_ptr<KeyedService> CreateKeyPermissionsManagerService(
      content::BrowserContext* context) {
    return std::make_unique<
        ash::platform_keys::FakeUserPrivateTokenKeyPermissionsManagerService>(
        &key_permissions_manager_);
  }

  // Derived classes can override this method to set the required authenticated
  // user in the IdentityManager class.
  virtual void SetAuthenticatedUser() {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->profile()), kUserEmail,
        signin::ConsentLevel::kSync);
  }

  // Like api_test_utils::RunFunctionAndReturnError but with an
  // explicit list of args.
  std::string RunFunctionAndReturnError(
      ExtensionFunction* function,
      base::Value::List args,
      content::BrowserContext* browser_context) {
    auto dispatcher =
        std::make_unique<ExtensionFunctionDispatcher>(browser_context);
    api_test_utils::RunFunction(
        function, std::move(args), std::move(dispatcher),
        extensions::api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
    return function->GetError();
  }

  // Like api_test_utils::RunFunctionAndReturnSingleResult but
  // with an explicit list of args.
  base::Value RunFunctionAndReturnSingleResult(
      ExtensionFunction* function,
      base::Value::List args,
      content::BrowserContext* browser_context) {
    scoped_refptr<ExtensionFunction> function_owner(function);
    // Without a callback the function will not generate a result.
    function->set_has_callback(true);
    auto dispatcher =
        std::make_unique<ExtensionFunctionDispatcher>(browser_context);
    api_test_utils::RunFunction(
        function, std::move(args), std::move(dispatcher),
        extensions::api_test_utils::FunctionMode::kNone);
    EXPECT_TRUE(function->GetError().empty())
        << "Unexpected error: " << function->GetError();
    if (function->GetResultListForTest() &&
        !function->GetResultListForTest()->empty()) {
      return (*function->GetResultListForTest())[0].Clone();
    }
    return base::Value();
  }

  scoped_refptr<const extensions::Extension> extension_;
  ash::StubInstallAttributes stub_install_attributes_;
  ash::platform_keys::MockKeyPermissionsManager key_permissions_manager_;
  raw_ptr<PrefService, DanglingUntriaged> prefs_ = nullptr;
  raw_ptr<ash::attestation::MockTpmChallengeKey, DanglingUntriaged>
      mock_tpm_challenge_key_ = nullptr;
};

class EPKChallengeMachineKeyTest : public EPKChallengeKeyTestBase {
 protected:
  EPKChallengeMachineKeyTest()
      : func_(base::MakeRefCounted<
              EnterprisePlatformKeysChallengeMachineKeyFunction>()) {
    func_->set_extension(extension_.get());
  }

  base::Value::List CreateArgs() { return CreateArgsInternal(std::nullopt); }

  base::Value::List CreateArgsNoRegister() {
    return CreateArgsInternal(base::Value(false));
  }

  base::Value::List CreateArgsRegister() {
    return CreateArgsInternal(base::Value(true));
  }

  base::Value::List CreateArgsInternal(
      std::optional<base::Value> register_key) {
    static constexpr std::string_view kData = "challenge";
    base::Value::List args;
    args.Append(base::Value(base::as_bytes(base::make_span(kData))));
    if (register_key) {
      args.Append(std::move(*register_key));
    }
    return args;
  }

  scoped_refptr<EnterprisePlatformKeysChallengeMachineKeyFunction> func_;
};

TEST_F(EPKChallengeMachineKeyTest, ExtensionNotAllowed) {
  base::Value::List empty_allowlist;
  prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                  std::move(empty_allowlist));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg,
      RunFunctionAndReturnError(func_.get(), CreateArgs(), profile()));
}

TEST_F(EPKChallengeMachineKeyTest, Success) {
  SetMockTpmChallenger();

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  base::Value value(
      RunFunctionAndReturnSingleResult(func_.get(), CreateArgs(), profile()));

  ASSERT_TRUE(value.is_blob());
  std::string response(value.GetBlob().begin(), value.GetBlob().end());
  EXPECT_EQ("response", response);
}

TEST_F(EPKChallengeMachineKeyTest, BadChallengeThenErrorMessageReturned) {
  SetMockTpmChallengerBadBase64Error();

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  base::Value value(
      RunFunctionAndReturnError(func_.get(), CreateArgs(), profile()));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kChallengeBadBase64ErrorMsg,
      value);
}

TEST_F(EPKChallengeMachineKeyTest, KeyNotRegisteredByDefault) {
  SetMockTpmChallenger();

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  EXPECT_CALL(*mock_tpm_challenge_key_, BuildResponse)
      .WillOnce(Invoke(FakeRunCheckNotRegister));

  auto dispatcher = std::make_unique<ExtensionFunctionDispatcher>(profile());
  EXPECT_TRUE(api_test_utils::RunFunction(
      func_.get(), CreateArgs(), std::move(dispatcher),
      extensions::api_test_utils::FunctionMode::kNone));
}

class EPKChallengeUserKeyTest : public EPKChallengeKeyTestBase {
 protected:
  EPKChallengeUserKeyTest()
      : func_(base::MakeRefCounted<
              EnterprisePlatformKeysChallengeUserKeyFunction>()) {
    func_->set_extension(extension_.get());
  }

  void SetUp() override { EPKChallengeKeyTestBase::SetUp(); }

  base::Value::List CreateArgs() { return CreateArgsInternal(true); }

  base::Value::List CreateArgsNoRegister() { return CreateArgsInternal(false); }

  base::Value::List CreateArgsInternal(bool register_key) {
    static constexpr std::string_view kData = "challenge";
    base::Value::List args;
    args.Append(base::Value(base::as_bytes(base::make_span(kData))));
    args.Append(register_key);
    return args;
  }

  EPKPChallengeKey impl_;
  scoped_refptr<EnterprisePlatformKeysChallengeUserKeyFunction> func_;
};

TEST_F(EPKChallengeUserKeyTest, Success) {
  SetMockTpmChallenger();

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  base::Value value(
      RunFunctionAndReturnSingleResult(func_.get(), CreateArgs(), profile()));

  ASSERT_TRUE(value.is_blob());
  std::string response(value.GetBlob().begin(), value.GetBlob().end());
  EXPECT_EQ("response", response);
}

TEST_F(EPKChallengeUserKeyTest, BadChallengeThenErrorMessageReturned) {
  SetMockTpmChallengerBadBase64Error();

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  base::Value value(
      RunFunctionAndReturnError(func_.get(), CreateArgs(), profile()));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kChallengeBadBase64ErrorMsg,
      value);
}

TEST_F(EPKChallengeUserKeyTest, ExtensionNotAllowedThenErrorMessageReturned) {
  base::Value::List empty_allowlist;
  prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                  std::move(empty_allowlist));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg,
      RunFunctionAndReturnError(func_.get(), CreateArgs(), profile()));
}

using EPKChallengeKeyParams =
    std::tuple<api::enterprise_platform_keys::Scope,
               std::optional<api::enterprise_platform_keys::Algorithm>>;

class EPKChallengeKeyTest
    : public EPKChallengeKeyTestBase,
      public testing::WithParamInterface<EPKChallengeKeyParams> {
 protected:
  EPKChallengeKeyTest()
      : func_(base::MakeRefCounted<
              EnterprisePlatformKeysChallengeKeyFunction>()) {
    func_->set_extension(extension_.get());
  }

  void AllowlistExtension() {
    base::Value::List allowlist;
    allowlist.Append(extension_->id());
    prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                    std::move(allowlist));
  }

  base::Value::List CreateArgs(
      std::optional<api::enterprise_platform_keys::RegisterKeyOptions>
          register_key,
      api::enterprise_platform_keys::Scope scope) {
    api::enterprise_platform_keys::ChallengeKeyOptions options;
    auto challenge = base::as_bytes(base::make_span("challenge"));
    options.challenge = std::vector(challenge.begin(), challenge.end());
    if (register_key.has_value()) {
      options.register_key.emplace(std::move(register_key.value()));
    }
    options.scope = scope;

    base::Value::List args;
    args.Append(options.ToValue());
    return args;
  }

  scoped_refptr<EnterprisePlatformKeysChallengeKeyFunction> func_;
  base::Value::List args_;
};

// This test ensures challengeKey propagates algorithm, scope, and registerKey
// parameters to the TpmChallengeKey class.
TEST_P(EPKChallengeKeyTest, Success) {
  SetMockTpmChallenger();
  AllowlistExtension();

  auto scope = std::get<0>(GetParam());
  ::attestation::VerifiedAccessFlow expected_va_flow_type;
  switch (scope) {
    case api::enterprise_platform_keys::Scope::kNone:
    case api::enterprise_platform_keys::Scope::kMachine:
      expected_va_flow_type = ::attestation::ENTERPRISE_MACHINE;
      break;
    case api::enterprise_platform_keys::Scope::kUser:
      expected_va_flow_type = ::attestation::ENTERPRISE_USER;
      break;
  }
  auto algorithm_opt = std::get<1>(GetParam());
  auto expect_register = algorithm_opt.has_value();
  auto expect_crypto_key_type = ::attestation::KEY_TYPE_RSA;
  std::optional<api::enterprise_platform_keys::RegisterKeyOptions>
      register_key = std::nullopt;
  if (algorithm_opt.has_value()) {
    switch (algorithm_opt.value()) {
      case api::enterprise_platform_keys::Algorithm::kNone:
      case api::enterprise_platform_keys::Algorithm::kRsa:
        expect_crypto_key_type = ::attestation::KEY_TYPE_RSA;
        break;
      case api::enterprise_platform_keys::Algorithm::kEcdsa:
        expect_crypto_key_type = ::attestation::KEY_TYPE_ECC;
        break;
    }
    register_key = api::enterprise_platform_keys::RegisterKeyOptions();
    register_key.value().algorithm = algorithm_opt.value();
  }

  EXPECT_CALL(*mock_tpm_challenge_key_,
              BuildResponse(expected_va_flow_type, _, _, _, expect_register,
                            expect_crypto_key_type, _, _));

  base::Value value(RunFunctionAndReturnSingleResult(
      func_.get(), CreateArgs(std::move(register_key), scope), profile()));

  ASSERT_TRUE(value.is_blob());
  std::string response(value.GetBlob().begin(), value.GetBlob().end());
  EXPECT_EQ("response", response);
}

// This test ensures challengeKey cannot be called by extensions not on the
// allow list.
TEST_P(EPKChallengeKeyTest, ExtensionNotAllowed) {
  base::Value::List empty_allowlist;
  prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                  std::move(empty_allowlist));

  auto scope = std::get<0>(GetParam());
  auto algorithm_opt = std::get<1>(GetParam());
  std::optional<api::enterprise_platform_keys::RegisterKeyOptions>
      register_key = std::nullopt;
  if (algorithm_opt.has_value()) {
    register_key = api::enterprise_platform_keys::RegisterKeyOptions();
    register_key.value().algorithm = algorithm_opt.value();
  }

  auto args = CreateArgs(std::move(register_key), scope);

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg,
      RunFunctionAndReturnError(func_.get(), std::move(args), profile()));
}

INSTANTIATE_TEST_SUITE_P(
    EPKChallengeKeyTests,
    EPKChallengeKeyTest,
    testing::Combine(
        testing::Values(api::enterprise_platform_keys::Scope::kMachine,
                        api::enterprise_platform_keys::Scope::kUser),
        testing::Values(api::enterprise_platform_keys::Algorithm::kRsa,
                        api::enterprise_platform_keys::Algorithm::kEcdsa,
                        std::nullopt)),

    [](const testing::TestParamInfo<EPKChallengeKeyParams>& info) {
      std::string alg =
          api::enterprise_platform_keys::ToString(std::get<0>(info.param));
      auto scope_opt = std::get<1>(info.param);

      std::string scope =
          scope_opt.has_value()
              ? api::enterprise_platform_keys::ToString(scope_opt.value())
              : "Unregistered";
      return std::string(alg) + scope;
    });

}  // namespace
}  // namespace extensions
