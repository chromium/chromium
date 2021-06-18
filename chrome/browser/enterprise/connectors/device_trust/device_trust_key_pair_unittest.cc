// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_key_pair.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class DeviceTrustKeyPairTest : public testing::Test {
 public:
  DeviceTrustKeyPairTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
    key_.Init();
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    testing::Test::TearDown();
  }

  DeviceTrustKeyPair key_;

 private:
  ScopedTestingLocalState local_state_;
};

TEST_F(DeviceTrustKeyPairTest, KeyPairCreateStoreLoad) {
  // Create the key pair, if will store the private part into prefs.
  EXPECT_TRUE(key_.Init());
  // Get public key.
  std::vector<uint8_t> public_key_info;
  EXPECT_TRUE(key_.ExportPublicKey(&public_key_info));
  // Create a new key pair, after call `Init` it will be created
  // from the private key info block stored in prefs.
  DeviceTrustKeyPair key2;
  EXPECT_TRUE(key2.Init());
  std::vector<uint8_t> public_key_info2;
  EXPECT_TRUE(key2.ExportPublicKey(&public_key_info2));
  // Check we have the same key pair.
  EXPECT_EQ(public_key_info, public_key_info2);
}

TEST_F(DeviceTrustKeyPairTest, MakeSignature) {
  // Create the key pair.
  EXPECT_TRUE(key_.Init());
  // Sign the message.
  std::string message = "data to be sign";
  std::string signature;
  EXPECT_TRUE(key_.SignMessage(message, &signature));
  // Verify signature with ECDSA_SHA256.
  std::vector<uint8_t> public_key_info;
  EXPECT_TRUE(key_.ExportPublicKey(&public_key_info));
  crypto::SignatureVerifier signature_verifier;
  EXPECT_TRUE(signature_verifier.VerifyInit(
      crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      std::vector<uint8_t>(signature.begin(), signature.end()),
      public_key_info));
  signature_verifier.VerifyUpdate(base::as_bytes(base::make_span(message)));
  EXPECT_TRUE(signature_verifier.VerifyFinal());
}

}  // namespace enterprise_connectors
