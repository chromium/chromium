// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;
}  // namespace

class RegistrationTokenHelperTest : public testing::Test {
 public:
  RegistrationTokenHelperTest() : unexportable_key_service_(task_manager_) {}

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

  void VerifyResult(const RegistrationTokenHelper::Result& result) {
    crypto::SignatureVerifier::SignatureAlgorithm algorithm =
        *unexportable_key_service().GetAlgorithm(result.binding_key_id);
    std::vector<uint8_t> pubkey =
        *unexportable_key_service().GetSubjectPublicKeyInfo(
            result.binding_key_id);

    EXPECT_TRUE(signin::VerifyJwtSignature(result.registration_token, algorithm,
                                           pubkey));
    EXPECT_EQ(result.wrapped_binding_key,
              unexportable_key_service().GetWrappedKey(result.binding_key_id));
  }

  unexportable_keys::UnexportableKeyId GenerateNewKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
        generate_future;
    unexportable_key_service().GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    RunBackgroundTasks();
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        key_id = generate_future.Get();
    CHECK(key_id.has_value());
    return *key_id;
  }

  std::vector<uint8_t> GetWrappedKey(
      const unexportable_keys::UnexportableKeyId& key_id) {
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        unexportable_key_service().GetWrappedKey(key_id);
    CHECK(wrapped_key.has_value());
    return *wrapped_key;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  unexportable_keys::UnexportableKeyTaskManager task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
};

TEST_F(RegistrationTokenHelperTest, SuccessForTokenBinding) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>> future;
  RegistrationTokenHelper helper(unexportable_key_service(),
                                 base::ToVector(kAcceptableAlgorithms));
  helper.GenerateForTokenBinding("test_client_id", "test_auth_code",
                                 GURL("https://accounts.google.com/Register"),
                                 future.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future.Get().has_value());
  VerifyResult(future.Get().value());
}

TEST_F(RegistrationTokenHelperTest, SuccessForTokenBindingReuseKey) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>> future;
  std::vector<uint8_t> wrapped_key = GetWrappedKey(GenerateNewKey());
  ASSERT_FALSE(wrapped_key.empty());
  RegistrationTokenHelper helper(unexportable_key_service(), wrapped_key);
  helper.GenerateForTokenBinding("test_client_id", "test_auth_code",
                                 GURL("https://accounts.google.com/Register"),
                                 future.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future.Get().has_value());
  VerifyResult(future.Get().value());
  EXPECT_EQ(future.Get()->wrapped_binding_key, wrapped_key);
}

TEST_F(RegistrationTokenHelperTest, SuccessForSessionBinding) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>> future;
  RegistrationTokenHelper helper(unexportable_key_service(),
                                 base::ToVector(kAcceptableAlgorithms));
  helper.GenerateForSessionBinding("test_challenge",
                                   GURL("https://accounts.google.com/Register"),
                                   future.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future.Get().has_value());
  VerifyResult(future.Get().value());
}

TEST_F(RegistrationTokenHelperTest, DoubleRegistration) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>>
      future_1;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>>
      future_2;
  RegistrationTokenHelper helper(unexportable_key_service(),
                                 base::ToVector(kAcceptableAlgorithms));
  helper.GenerateForTokenBinding("client_id_1", "auth_code_1",
                                 GURL("https://accounts.google.com/Register1"),
                                 future_1.GetCallback());
  helper.GenerateForTokenBinding("client_id_2", "auth_code_2",
                                 GURL("https://accounts.google.com/Register2"),
                                 future_2.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future_1.Get().has_value());
  ASSERT_TRUE(future_2.Get().has_value());
  VerifyResult(future_1.Get().value());
  VerifyResult(future_2.Get().value());
  // Both registrations should use the same key.
  EXPECT_EQ(future_1.Get()->binding_key_id, future_2.Get()->binding_key_id);
}

TEST_F(RegistrationTokenHelperTest, Failure) {
  // Emulates key generation failure.
  crypto::ScopedNullUnexportableKeyProvider scoped_null_key_provider_;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>> future;
  RegistrationTokenHelper helper(unexportable_key_service(),
                                 base::ToVector(kAcceptableAlgorithms));
  helper.GenerateForTokenBinding("test_client_id", "test_auth_code",
                                 GURL("https://accounts.google.com/Register"),
                                 future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(RegistrationTokenHelperTest, FailureReuseKey) {
  const std::vector<uint8_t> kInvalidWrappedKey = {1, 2, 3};
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>> future;
  RegistrationTokenHelper helper(unexportable_key_service(),
                                 kInvalidWrappedKey);
  helper.GenerateForTokenBinding("test_client_id", "test_auth_code",
                                 GURL("https://accounts.google.com/Register"),
                                 future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(RegistrationTokenHelperTest, FailureEmptyAlgorithms) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<std::optional<RegistrationTokenHelper::Result>> future;
  RegistrationTokenHelper helper(
      unexportable_key_service(),
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>());
  helper.GenerateForTokenBinding("test_client_id", "test_auth_code",
                                 GURL("https://accounts.google.com/Register"),
                                 future.GetCallback());
  RunBackgroundTasks();
  ASSERT_FALSE(future.Get().has_value());
}
