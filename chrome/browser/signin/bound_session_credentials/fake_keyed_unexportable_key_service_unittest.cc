// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/fake_keyed_unexportable_key_service.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserVisible;
constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};

TEST(FakeKeyedUnexportableKeyServiceTest, CreateBasic) {
  FakeKeyedUnexportableKeyService fake_service;
}

TEST(FakeKeyedUnexportableKeyServiceTest, GetSubjectPublicKeyInfo) {
  FakeKeyedUnexportableKeyService fake_service;
  unexportable_keys::UnexportableKeyId key_id;
  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> result =
      fake_service.GetSubjectPublicKeyInfo(key_id);
  ASSERT_FALSE(result.has_value());
  ASSERT_THAT(result.error(), unexportable_keys::ServiceError::kKeyNotFound);
}

TEST(FakeKeyedUnexportableKeyServiceTest, GetWrappedKey) {
  FakeKeyedUnexportableKeyService fake_service;
  unexportable_keys::UnexportableKeyId key_id;
  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> result =
      fake_service.GetWrappedKey(key_id);
  ASSERT_FALSE(result.has_value());
  ASSERT_THAT(result.error(), unexportable_keys::ServiceError::kKeyNotFound);
}

TEST(FakeKeyedUnexportableKeyServiceTest, GetAlgorithm) {
  FakeKeyedUnexportableKeyService fake_service;
  unexportable_keys::UnexportableKeyId key_id;
  unexportable_keys::ServiceErrorOr<
      crypto::SignatureVerifier::SignatureAlgorithm>
      result = fake_service.GetAlgorithm(key_id);
  ASSERT_FALSE(result.has_value());
  ASSERT_THAT(result.error(), unexportable_keys::ServiceError::kKeyNotFound);
}

TEST(FakeKeyedUnexportableKeyServiceTest, GenerateSigningKeySlowlyAsync) {
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      future;
  FakeKeyedUnexportableKeyService fake_service;
  fake_service.GenerateSigningKeySlowlyAsync(
      kAcceptableAlgorithms, kTaskPriority, future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
      result = future.Get();
  ASSERT_THAT(result.error(), unexportable_keys::ServiceError::kKeyNotFound);
}

TEST(FakeKeyedUnexportableKeyServiceTest, FromWrappedSigningKeySlowlyAsync) {
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      future;
  FakeKeyedUnexportableKeyService fake_service;
  std::vector<uint8_t> wrapped_key;
  fake_service.FromWrappedSigningKeySlowlyAsync(wrapped_key, kTaskPriority,
                                                future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
      result = future.Get();
  ASSERT_THAT(result.error(), unexportable_keys::ServiceError::kKeyNotFound);
}

TEST(FakeKeyedUnexportableKeyServiceTest, SignSlowlyAsync) {
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<std::vector<uint8_t>>>
      future;
  FakeKeyedUnexportableKeyService fake_service;
  unexportable_keys::UnexportableKeyId key_id;
  std::vector<uint8_t> data = {1, 2, 3};
  fake_service.SignSlowlyAsync(key_id, data, kTaskPriority,
                               future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> result = future.Get();
  ASSERT_THAT(result.error(), unexportable_keys::ServiceError::kKeyNotFound);
}

}  // namespace
