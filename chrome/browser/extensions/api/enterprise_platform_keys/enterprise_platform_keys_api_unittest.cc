// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/chromeos/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;

namespace utils = extension_function_test_utils;

namespace extensions {
namespace {

const char kUserEmail[] = "test@google.com";

void FakeRunCheckNotRegister(
    chromeos::attestation::AttestationKeyType key_type,
    Profile* profile,
    chromeos::attestation::TpmChallengeKeyCallback callback,
    const std::string& challenge,
    bool register_key,
    const std::string& key_name_for_spkac) {
  EXPECT_FALSE(register_key);
  std::move(callback).Run(
      chromeos::attestation::TpmChallengeKeyResult::MakeResult("response"));
}

class EPKChallengeKeyTestBase : public BrowserWithTestWindowTest {
 protected:
  EPKChallengeKeyTestBase()
      : extension_(ExtensionBuilder("Test").Build()),
        fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {
    stub_install_attributes_.SetCloudManaged("google.com", "device_id");
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    prefs_ = browser()->profile()->GetPrefs();
    SetAuthenticatedUser();
  }

  void SetMockTpmChallenger() {
    auto mock_tpm_challenge_key = std::make_unique<
        NiceMock<chromeos::attestation::MockTpmChallengeKey>>();
    // Will be used with EXPECT_CALL.
    mock_tpm_challenge_key_ = mock_tpm_challenge_key.get();
    mock_tpm_challenge_key->EnableFake();
    // transfer ownership inside factory
    chromeos::attestation::TpmChallengeKeyFactory::SetForTesting(
        std::move(mock_tpm_challenge_key));
  }

  // This will be called by BrowserWithTestWindowTest::SetUp();
  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUserWithAffiliation(
        AccountId::FromUserEmail(kUserEmail), true);
    return profile_manager()->CreateTestingProfile(kUserEmail);
  }

  // Derived classes can override this method to set the required authenticated
  // user in the IdentityManager class.
  virtual void SetAuthenticatedUser() {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->profile()),
        kUserEmail);
  }

  // Like extension_function_test_utils::RunFunctionAndReturnError but with an
  // explicit ListValue.
  std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                        std::unique_ptr<base::ListValue> args,
                                        Browser* browser) {
    utils::RunFunction(function, std::move(args), browser,
                       extensions::api_test_utils::NONE);
    EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
    return function->GetError();
  }

  // Like extension_function_test_utils::RunFunctionAndReturnSingleResult but
  // with an explicit ListValue.
  base::Value* RunFunctionAndReturnSingleResult(
      ExtensionFunction* function,
      std::unique_ptr<base::ListValue> args,
      Browser* browser) {
    scoped_refptr<ExtensionFunction> function_owner(function);
    // Without a callback the function will not generate a result.
    function->set_has_callback(true);
    utils::RunFunction(function, std::move(args), browser,
                       extensions::api_test_utils::NONE);
    EXPECT_TRUE(function->GetError().empty())
        << "Unexpected error: " << function->GetError();
    const base::Value* single_result = NULL;
    if (function->GetResultList() != NULL &&
        function->GetResultList()->Get(0, &single_result)) {
      return single_result->DeepCopy();
    }
    return NULL;
  }

  scoped_refptr<const extensions::Extension> extension_;
  chromeos::StubInstallAttributes stub_install_attributes_;
  // fake_user_manager_ is owned by user_manager_enabler_.
  chromeos::FakeChromeUserManager* fake_user_manager_ = nullptr;
  user_manager::ScopedUserManager user_manager_enabler_;
  PrefService* prefs_ = nullptr;
  chromeos::attestation::MockTpmChallengeKey* mock_tpm_challenge_key_ = nullptr;
};

class EPKChallengeMachineKeyTest : public EPKChallengeKeyTestBase {
 protected:
  EPKChallengeMachineKeyTest()
      : func_(new EnterprisePlatformKeysChallengeMachineKeyFunction()) {
    func_->set_extension(extension_.get());
  }

  std::unique_ptr<base::ListValue> CreateArgs() {
    return CreateArgsInternal(nullptr);
  }

  std::unique_ptr<base::ListValue> CreateArgsNoRegister() {
    return CreateArgsInternal(std::make_unique<bool>(false));
  }

  std::unique_ptr<base::ListValue> CreateArgsRegister() {
    return CreateArgsInternal(std::make_unique<bool>(true));
  }

  std::unique_ptr<base::ListValue> CreateArgsInternal(
      std::unique_ptr<bool> register_key) {
    std::unique_ptr<base::ListValue> args(new base::ListValue);
    args->Append(base::Value::CreateWithCopiedBuffer("challenge", 9));
    if (register_key) {
      args->AppendBoolean(*register_key);
    }
    return args;
  }

  scoped_refptr<EnterprisePlatformKeysChallengeMachineKeyFunction> func_;
  base::ListValue args_;
};

TEST_F(EPKChallengeMachineKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(chromeos::attestation::TpmChallengeKeyResult::
                kExtensionNotWhitelistedErrorMsg,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, Success) {
  SetMockTpmChallenger();

  base::Value whitelist(base::Value::Type::LIST);
  whitelist.Append(extension_->id());
  prefs_->Set(prefs::kAttestationExtensionWhitelist, whitelist);

  std::unique_ptr<base::Value> value(
      RunFunctionAndReturnSingleResult(func_.get(), CreateArgs(), browser()));

  ASSERT_TRUE(value->is_blob());
  std::string response(value->GetBlob().begin(), value->GetBlob().end());
  EXPECT_EQ("response", response);
}

TEST_F(EPKChallengeMachineKeyTest, KeyNotRegisteredByDefault) {
  SetMockTpmChallenger();

  base::ListValue whitelist;
  whitelist.AppendString(extension_->id());
  prefs_->Set(prefs::kAttestationExtensionWhitelist, whitelist);

  EXPECT_CALL(*mock_tpm_challenge_key_, BuildResponse)
      .WillOnce(Invoke(FakeRunCheckNotRegister));

  EXPECT_TRUE(utils::RunFunction(func_.get(), CreateArgs(), browser(),
                                 extensions::api_test_utils::NONE));
}

class EPKChallengeUserKeyTest : public EPKChallengeKeyTestBase {
 protected:
  EPKChallengeUserKeyTest()
      : func_(new EnterprisePlatformKeysChallengeUserKeyFunction()) {
    func_->set_extension(extension_.get());
  }

  void SetUp() override {
    EPKChallengeKeyTestBase::SetUp();

    // Set the user preferences.
    prefs_->SetBoolean(prefs::kAttestationEnabled, true);
  }

  std::unique_ptr<base::ListValue> CreateArgs() {
    return CreateArgsInternal(true);
  }

  std::unique_ptr<base::ListValue> CreateArgsNoRegister() {
    return CreateArgsInternal(false);
  }

  std::unique_ptr<base::ListValue> CreateArgsInternal(bool register_key) {
    std::unique_ptr<base::ListValue> args(new base::ListValue);
    args->Append(base::Value::CreateWithCopiedBuffer("challenge", 9));
    args->AppendBoolean(register_key);
    return args;
  }

  EPKPChallengeKey impl_;
  scoped_refptr<EnterprisePlatformKeysChallengeUserKeyFunction> func_;
};

TEST_F(EPKChallengeUserKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(chromeos::attestation::TpmChallengeKeyResult::
                kExtensionNotWhitelistedErrorMsg,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

}  // namespace
}  // namespace extensions
