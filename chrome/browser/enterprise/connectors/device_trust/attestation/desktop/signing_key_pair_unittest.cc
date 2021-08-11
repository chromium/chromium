// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

#include "build/build_config.h"

#if defined(OS_WIN)

#include "base/base64.h"
#include "base/test/test_reg_util_win.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

#else

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

#endif  // !defined(OS_WIN)

namespace enterprise_connectors {

#if defined(OS_WIN)

class SigningKeyPairTest : public testing::Test {
 public:
  SigningKeyPairTest() {
    EXPECT_NO_FATAL_FAILURE(
        registry_override_manager()->OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  policy::FakeBrowserDMTokenStorage* fake_browser_dm_storage() {
    return &fake_browser_dm_storage_;
  }

  registry_util::RegistryOverrideManager* registry_override_manager() {
    return &reg_mananger_;
  }

 private:
  registry_util::RegistryOverrideManager reg_mananger_;
  policy::FakeBrowserDMTokenStorage fake_browser_dm_storage_;
};

TEST_F(SigningKeyPairTest, NoKeyPair) {
  SigningKeyPair key;

  // No key in the registry, so there should be no public key.
  std::vector<uint8_t> pubkey;
  ASSERT_FALSE(key.ExportPublicKey(&pubkey));

  // No key in the registry, so can't sign.
  ASSERT_FALSE(key.SignMessage("data to sign", nullptr));
}

TEST_F(SigningKeyPairTest, WithKeyPair) {
  // Set up browser as if it were CBCM enrolled.
  fake_browser_dm_storage()->SetClientId("fake_client_id");
  fake_browser_dm_storage()->SetDMToken("fake_dm_token");

  std::string dm_token_base64;
  base::Base64Encode("fake_dm_token", &dm_token_base64);

  // Create a new key and save it.
  SigningKeyPair key;
  ASSERT_TRUE(key.RotateWithElevation(dm_token_base64));

  // Extract a pubkey should work now.
  std::vector<uint8_t> pubkey;
  ASSERT_TRUE(key.ExportPublicKey(&pubkey));
  ASSERT_GT(pubkey.size(), 0u);

  // Signing should work now.
  std::string signature;
  ASSERT_TRUE(key.SignMessage("data to sign", &signature));
  ASSERT_GT(signature.size(), 0u);
}

#else

class SigningKeyPairTest : public testing::Test {
 public:
  SigningKeyPairTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

 private:
  ScopedTestingLocalState local_state_;
};

TEST_F(SigningKeyPairTest, KeyPairCreateStoreLoad) {
  SigningKeyPair key;

  std::vector<uint8_t> public_keyinfo;
  EXPECT_TRUE(key.ExportPublicKey(&public_keyinfo));
  // Create a new key pair, it will be created from the private key info block
  // stored in prefs.
  SigningKeyPair key2;
  std::vector<uint8_t> public_keyinfo2;
  EXPECT_TRUE(key2.ExportPublicKey(&public_keyinfo2));
  // Check we have the same key pair.
  EXPECT_EQ(public_keyinfo, public_keyinfo2);
}

TEST_F(SigningKeyPairTest, MakeSignature) {
  SigningKeyPair key;

  // Sign the message.
  std::string message = "data to be sign";
  std::string signature;
  EXPECT_TRUE(key.SignMessage(message, &signature));
  // Verify signature with ECDSA_SHA256.
  std::vector<uint8_t> public_keyinfo;
  EXPECT_TRUE(key.ExportPublicKey(&public_keyinfo));
  crypto::SignatureVerifier signature_verifier;
  EXPECT_TRUE(signature_verifier.VerifyInit(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      std::vector<uint8_t>(signature.begin(), signature.end()),
      public_keyinfo));
  signature_verifier.VerifyUpdate(base::as_bytes(base::make_span(message)));
  EXPECT_TRUE(signature_verifier.VerifyFinal());
}

#endif  // !defined(OS_WIN)

}  // namespace enterprise_connectors
