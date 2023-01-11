// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
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

using testing::NiceMock;

namespace utils = extension_function_test_utils;

namespace extensions {
namespace {

const char kUserEmail[] = "test@google.com";

class EPKPChallengeKeyTestBase : public BrowserWithTestWindowTest {
 protected:
  EPKPChallengeKeyTestBase()
      : fake_user_manager_(new ash::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {
    extension_ = ExtensionBuilder("Test").Build();
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    prefs_ = browser()->profile()->GetPrefs();
    SetAuthenticatedUser();
  }

  void SetMockTpmChallenger() {
    auto mock_tpm_challenge_key =
        std::make_unique<NiceMock<ash::attestation::MockTpmChallengeKey>>();
    mock_tpm_challenge_key->EnableFake();
    ash::attestation::TpmChallengeKeyFactory::SetForTesting(
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
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, kUserEmail,
                                        signin::ConsentLevel::kSync);
  }

  scoped_refptr<const Extension> extension_;
  // fake_user_manager_ is owned by user_manager_enabler_.
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  user_manager::ScopedUserManager user_manager_enabler_;
  PrefService* prefs_ = nullptr;
};

class EPKPChallengeMachineKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kFuncArgs[];

  EPKPChallengeMachineKeyTest()
      : func_(new EnterprisePlatformKeysPrivateChallengeMachineKeyFunction()) {
    func_->set_extension(extension_.get());
  }

  scoped_refptr<EnterprisePlatformKeysPrivateChallengeMachineKeyFunction> func_;
};

// Base 64 encoding of 'challenge'.
const char EPKPChallengeMachineKeyTest::kFuncArgs[] = "[\"Y2hhbGxlbmdl\"]";

TEST_F(EPKPChallengeMachineKeyTest, ExtensionNotAllowlisted) {
  base::Value::List empty_allowlist;
  prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                  std::move(empty_allowlist));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg,
      utils::RunFunctionAndReturnError(func_.get(), kFuncArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, Success) {
  SetMockTpmChallenger();

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func_.get(), kFuncArgs, browser(), extensions::api_test_utils::NONE));

  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */,
            value->GetString());
}

class EPKPChallengeUserKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kFuncArgs[];

  EPKPChallengeUserKeyTest()
      : func_(new EnterprisePlatformKeysPrivateChallengeUserKeyFunction()) {
    func_->set_extension(extension_.get());
  }

  scoped_refptr<EnterprisePlatformKeysPrivateChallengeUserKeyFunction> func_;
};

// Base 64 encoding of 'challenge', register_key required.
const char EPKPChallengeUserKeyTest::kFuncArgs[] = "[\"Y2hhbGxlbmdl\", true]";

TEST_F(EPKPChallengeUserKeyTest, ExtensionNotAllowlisted) {
  base::Value::List empty_allowlist;
  prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                  std::move(empty_allowlist));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg,
      utils::RunFunctionAndReturnError(func_.get(), kFuncArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, Success) {
  SetMockTpmChallenger();

  base::Value::List allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func_.get(), kFuncArgs, browser(), extensions::api_test_utils::NONE));

  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */,
            value->GetString());
}

}  // namespace
}  // namespace extensions
