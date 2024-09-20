// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/helpers/key_helper.h"

#include <pk11pub.h>

#include <vector>

#include "base/base64.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace kcer::internal {
namespace {

// Tests for helper methods from key_helper.h. Methods are mainly tested from
// the higher level pkcs12_reader_unittest.cc and from higher level
// chaps_util_impl_unittest.cc. However some corner cases can be tested only
// with manually crafted input data, and this is done from here.
class KeyHelperTest : public ::testing::Test {
 public:
  KeyHelperTest() = default;
  KeyHelperTest(const KeyHelperTest&) = delete;
  KeyHelperTest& operator=(const KeyHelperTest&) = delete;
  ~KeyHelperTest() override = default;

  void SetEcPrivateKey(const char* new_key, bssl::UniquePtr<EVP_PKEY>& pkey) {
    bssl::UniquePtr<BIGNUM> priv_key_bn(BN_new());
    BIGNUM* priv_key_bn_ptr = priv_key_bn.get();
    CHECK(BN_hex2bn(&priv_key_bn_ptr, new_key));
    EC_KEY* priv_key = EVP_PKEY_get0_EC_KEY(pkey.get());
    CHECK(EC_KEY_set_private_key(priv_key, priv_key_bn_ptr));
  }

  bssl::UniquePtr<EVP_PKEY> GenerateEcKey() {
    bssl::UniquePtr<EVP_PKEY_CTX> ctx(
        EVP_PKEY_CTX_new_id(EVP_PKEY_EC, /*e=*/nullptr));
    CHECK(EVP_PKEY_keygen_init(ctx.get()));
    CHECK(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(),
                                                 NID_X9_62_prime256v1));
    EVP_PKEY* key = nullptr;
    CHECK(EVP_PKEY_keygen(ctx.get(), &key));
    return bssl::UniquePtr<EVP_PKEY>(key);
  }
};

TEST_F(KeyHelperTest, NoEcKeyGetEcPrivateKeyBytesReturnsEmpty) {
  std::vector<uint8_t> result = GetEcPrivateKeyBytes(nullptr);

  EXPECT_TRUE(result.empty());
}

TEST_F(KeyHelperTest, GetEcPrivateKeyBytes) {
  // Regular EC key_data, private key has leading zeros. Leading zeros will be
  // not trimmed. Operation will succeed.
  bssl::UniquePtr<EVP_PKEY> key = GenerateEcKey();

  // Make sure that generated EC key has correct length for the private key.
  // We use NID_X9_62_prime256v1, expected private key length is 256 bits.
  unsigned long expected_key_length_bytes = 32;
  // Hex 'aa' will be set to private key and it will be  expanded to 256 bits
  // with leading zeros.
  char new_priv_key[] = "AA";  // Hex 'AA' = dec '170'.
  SetEcPrivateKey(new_priv_key, key);
  std::vector<uint8_t> expected_priv_key = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 170,
  };
  CHECK(expected_priv_key.size() == expected_key_length_bytes);

  std::vector<uint8_t> result =
      GetEcPrivateKeyBytes(EVP_PKEY_get0_EC_KEY(key.get()));

  EXPECT_EQ(expected_key_length_bytes, result.size());
  EXPECT_EQ(result, expected_priv_key);
}

}  // namespace
}  // namespace kcer::internal
