// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/payload_util.h"

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "chrome/browser/tab/storage_id.h"
#include "crypto/aead.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
namespace {

TEST(PayloadUtilTest, GenerateKeyForOtrPayloads) {
  std::vector<uint8_t> key1 = GenerateKeyForOtrPayloads();
  std::vector<uint8_t> key2 = GenerateKeyForOtrPayloads();
  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  ASSERT_EQ(key1.size(), aead.KeyLength());
  ASSERT_EQ(key2.size(), aead.KeyLength());
  ASSERT_NE(key1, key2);
}

TEST(PayloadUtilTest, SealAndOpenPayload) {
  std::vector<uint8_t> key = GenerateKeyForOtrPayloads();
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  StorageId storage_id = StorageId::Create();

  std::vector<uint8_t> sealed_payload = SealPayload(key, payload, storage_id);
  ASSERT_FALSE(sealed_payload.empty());

  std::optional<std::vector<uint8_t>> opened_payload =
      OpenPayload(key, sealed_payload, storage_id);
  ASSERT_TRUE(opened_payload.has_value());
  ASSERT_EQ(opened_payload.value(), payload);
}

TEST(PayloadUtilTest, OpenPayloadWithBadKey) {
  std::vector<uint8_t> key = GenerateKeyForOtrPayloads();
  std::vector<uint8_t> bad_key = GenerateKeyForOtrPayloads();
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  StorageId storage_id = StorageId::Create();

  std::vector<uint8_t> sealed_payload = SealPayload(key, payload, storage_id);
  ASSERT_FALSE(sealed_payload.empty());

  std::optional<std::vector<uint8_t>> opened_payload =
      OpenPayload(bad_key, sealed_payload, storage_id);
  ASSERT_FALSE(opened_payload.has_value());
}

TEST(PayloadUtilTest, OpenPayloadWithTruncatedPayload) {
  std::vector<uint8_t> key = GenerateKeyForOtrPayloads();
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  StorageId storage_id = StorageId::Create();

  std::vector<uint8_t> sealed_payload = SealPayload(key, payload, storage_id);
  ASSERT_FALSE(sealed_payload.empty());

  // Truncate the sealed payload by 1 byte (removing part of the nonce).
  sealed_payload.pop_back();

  std::optional<std::vector<uint8_t>> opened_payload =
      OpenPayload(key, sealed_payload, storage_id);
  ASSERT_FALSE(opened_payload.has_value());
}

TEST(PayloadUtilTest, SealAndOpenEmptyPayload) {
  std::vector<uint8_t> key = GenerateKeyForOtrPayloads();
  std::vector<uint8_t> payload = {};
  StorageId storage_id = StorageId::Create();

  std::vector<uint8_t> sealed_payload = SealPayload(key, payload, storage_id);
  ASSERT_FALSE(sealed_payload.empty());

  std::optional<std::vector<uint8_t>> opened_payload =
      OpenPayload(key, sealed_payload, storage_id);
  ASSERT_TRUE(opened_payload.has_value());
  ASSERT_TRUE(opened_payload.value().empty());
}

}  // namespace
}  // namespace tabs
