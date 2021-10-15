// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <memory>
#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/memory_signing_key_pair.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

namespace enterprise_connectors {

class SigningKeyPairTest : public testing::Test {
 protected:
  SigningKeyPair* key_pair() { return key_pair_.get(); }

  void set_force_store_to_fail(bool force_store_to_fail) {
    ppdelegate_ptr->set_force_store_to_fail(force_store_to_fail);
  }

  void push_response_codes(
      const std::deque<BPKUP::ResponseCode>& response_codes) {
    pndelegate_ptr->push_response_codes(response_codes);
  }

  test::InMemorySigningKeyPairNetworkDelegate* network_delegate() {
    return pndelegate_ptr;
  }

 private:
  test::ScopedMemorySigningKeyPairPersistence persistence_scope_;
  test::InMemorySigningKeyPairPersistenceDelegate* ppdelegate_ptr = nullptr;
  test::InMemorySigningKeyPairNetworkDelegate* pndelegate_ptr = nullptr;
  std::unique_ptr<SigningKeyPair> key_pair_ =
      test::CreateInMemorySigningKeyPair(&ppdelegate_ptr, &pndelegate_ptr);
};

TEST_F(SigningKeyPairTest, NoKeyPair) {
  // No persisted key, so there should be no public key.
  std::vector<uint8_t> pubkey;
  ASSERT_FALSE(key_pair()->ExportPublicKey(&pubkey));

  // No persisted key, so can't sign.
  ASSERT_FALSE(key_pair()->SignMessage("data to sign", nullptr));
}

TEST_F(SigningKeyPairTest, Rotate) {
  // Create a new key and save it.
  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));
}

TEST_F(SigningKeyPairTest, ExportAndSign) {
  // Create a new key and save it.
  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));

  // Extract a pubkey should work now.
  std::vector<uint8_t> pubkey;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey));
  ASSERT_GT(pubkey.size(), 0u);

  // Signing should work now.
  std::string signature;
  ASSERT_TRUE(key_pair()->SignMessage("data to sign", &signature));
  ASSERT_GT(signature.size(), 0u);
}

TEST_F(SigningKeyPairTest, Load) {
  // Create a new key and save it.
  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));

  // Extract a pubkey should work now.
  std::vector<uint8_t> pubkey;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey));
  ASSERT_GT(pubkey.size(), 0u);

  // A new key pair should load the same key.
  auto key_pair2 = test::CreateInMemorySigningKeyPair(nullptr, nullptr);
  std::vector<uint8_t> pubkey2;
  ASSERT_TRUE(key_pair2->ExportPublicKey(&pubkey2));
  ASSERT_GT(pubkey2.size(), 0u);

  ASSERT_EQ(pubkey, pubkey2);
}

TEST_F(SigningKeyPairTest, FailedSaveKeepsOldKey) {
  // Create a new key and save it.
  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));

  // Extract a pubkey should work now.
  std::vector<uint8_t> pubkey;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey));
  ASSERT_GT(pubkey.size(), 0u);

  set_force_store_to_fail(true);

  ASSERT_FALSE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                 "fake_dm_token", "nonce"));
  std::vector<uint8_t> pubkey2;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey2));
  ASSERT_EQ(pubkey, pubkey2);
}

TEST_F(SigningKeyPairTest, FailedNetworkKeepsOldKey) {
  // Create a new key and save it.
  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));

  // Extract a pubkey should work now.
  std::vector<uint8_t> pubkey;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey));
  ASSERT_GT(pubkey.size(), 0u);

  push_response_codes({BPKUP::INVALID_SIGNATURE});

  ASSERT_FALSE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                 "fake_dm_token", "nonce"));
  std::vector<uint8_t> pubkey2;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey2));
  ASSERT_EQ(pubkey, pubkey2);

  // Send count should be 2: one for the initial rotate that works, and a
  // second for the rotate that fails.
  EXPECT_EQ(network_delegate()->send_count(), 2);
}

TEST_F(SigningKeyPairTest, RetryAndSuccessNetworkKeepsNewKey) {
  // Create a new key and save it.
  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));

  // Extract a pubkey should work now.
  std::vector<uint8_t> pubkey;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey));
  ASSERT_GT(pubkey.size(), 0u);

  push_response_codes({BPKUP::UNDEFINED, BPKUP::SUCCESS});

  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));
  std::vector<uint8_t> pubkey2;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey2));
  ASSERT_NE(pubkey, pubkey2);

  // Send count should be 3: one for the initial rotate that works and two
  // more for the rotate that fails with retries.
  EXPECT_EQ(network_delegate()->send_count(), 3);
}

TEST_F(SigningKeyPairTest, RetryAndFailedNetworkKeepsOldKey) {
  // Create a new key and save it.
  ASSERT_TRUE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                "fake_dm_token", "nonce"));

  // Extract a pubkey should work now.
  std::vector<uint8_t> pubkey;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey));
  ASSERT_GT(pubkey.size(), 0u);

  push_response_codes(
      {BPKUP::UNDEFINED, BPKUP::UNDEFINED, BPKUP::INVALID_SIGNATURE});

  ASSERT_FALSE(key_pair()->RotateWithAdminRights(GURL("dmserver.com"),
                                                 "fake_dm_token", "nonce"));
  std::vector<uint8_t> pubkey2;
  ASSERT_TRUE(key_pair()->ExportPublicKey(&pubkey2));
  ASSERT_EQ(pubkey, pubkey2);

  // Send count should be 4: one for the initial rotate that works and three
  // more for the rotate that fails with retries.
  EXPECT_EQ(network_delegate()->send_count(), 4);
}

}  // namespace enterprise_connectors
