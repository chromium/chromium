// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace extensions {

namespace utils = api_test_utils;

namespace {

const char kUserEmail[] = "test@google.com";

class EPKPChallengeKeyTestBase : public ExtensionApiTest {
 protected:
  EPKPChallengeKeyTestBase() = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    extension_ = ExtensionBuilder("Test").Build();
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

  // Derived classes can override this method to set the required authenticated
  // user in the IdentityManager class.
  virtual void SetAuthenticatedUser() {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, kUserEmail,
                                        signin::ConsentLevel::kSync);
  }

  scoped_refptr<const Extension> extension_;
  raw_ptr<PrefService, DanglingUntriaged> prefs_ = nullptr;
};

class EPKPChallengeMachineKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kFuncArgs[];

  EPKPChallengeMachineKeyTest() = default;

  void SetUpOnMainThread() override {
    EPKPChallengeKeyTestBase::SetUpOnMainThread();
    func_ = base::MakeRefCounted<
        EnterprisePlatformKeysPrivateChallengeMachineKeyFunction>();
    func_->set_extension(extension_.get());
  }

  scoped_refptr<EnterprisePlatformKeysPrivateChallengeMachineKeyFunction> func_;
};

// Base 64 encoding of 'challenge'.
const char EPKPChallengeMachineKeyTest::kFuncArgs[] = "[\"Y2hhbGxlbmdl\"]";

IN_PROC_BROWSER_TEST_F(EPKPChallengeMachineKeyTest, ExtensionNotAllowlisted) {
  base::ListValue empty_allowlist;
  prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                  std::move(empty_allowlist));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg,
      utils::RunFunctionAndReturnError(func_.get(), kFuncArgs,
                                       browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(EPKPChallengeMachineKeyTest, Success) {
  SetMockTpmChallenger();

  base::ListValue allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
      func_.get(), kFuncArgs, browser()->profile(),
      extensions::api_test_utils::FunctionMode::kNone);

  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */,
            value->GetString());
}

class EPKPChallengeUserKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kFuncArgs[];

  EPKPChallengeUserKeyTest() = default;

  void SetUpOnMainThread() override {
    EPKPChallengeKeyTestBase::SetUpOnMainThread();
    func_ = base::MakeRefCounted<
        EnterprisePlatformKeysPrivateChallengeUserKeyFunction>();
    func_->set_extension(extension_.get());
  }

  scoped_refptr<EnterprisePlatformKeysPrivateChallengeUserKeyFunction> func_;
};

// Base 64 encoding of 'challenge', register_key required.
const char EPKPChallengeUserKeyTest::kFuncArgs[] = "[\"Y2hhbGxlbmdl\", true]";

IN_PROC_BROWSER_TEST_F(EPKPChallengeUserKeyTest, ExtensionNotAllowlisted) {
  base::ListValue empty_allowlist;
  prefs_->SetList(prefs::kAttestationExtensionAllowlist,
                  std::move(empty_allowlist));

  EXPECT_EQ(
      ash::attestation::TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg,
      utils::RunFunctionAndReturnError(func_.get(), kFuncArgs,
                                       browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(EPKPChallengeUserKeyTest, Success) {
  SetMockTpmChallenger();

  base::ListValue allowlist;
  allowlist.Append(extension_->id());
  prefs_->SetList(prefs::kAttestationExtensionAllowlist, std::move(allowlist));

  std::optional<base::Value> value = utils::RunFunctionAndReturnSingleResult(
      func_.get(), kFuncArgs, browser()->profile(),
      extensions::api_test_utils::FunctionMode::kNone);

  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */,
            value->GetString());
}

}  // namespace
}  // namespace extensions
