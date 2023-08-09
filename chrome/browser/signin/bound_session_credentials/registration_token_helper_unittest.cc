// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"

#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

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

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
};

TEST_F(RegistrationTokenHelperTest, SuccessForTokenBinding) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<absl::optional<RegistrationTokenHelper::Result>>
      future;
  std::unique_ptr<RegistrationTokenHelper> helper =
      RegistrationTokenHelper::CreateForTokenBinding(
          unexportable_key_service(), "test_client_id", "test_auth_code",
          GURL("https://accounts.google.com/Register"), future.GetCallback());
  helper->Start();
  RunBackgroundTasks();
  ASSERT_TRUE(future.Get().has_value());
  VerifyResult(future.Get().value());
}

TEST_F(RegistrationTokenHelperTest, SuccessForSessionBinding) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  base::test::TestFuture<absl::optional<RegistrationTokenHelper::Result>>
      future;
  std::unique_ptr<RegistrationTokenHelper> helper =
      RegistrationTokenHelper::CreateForSessionBinding(
          unexportable_key_service(), "test_challenge",
          GURL("https://accounts.google.com/Register"), future.GetCallback());
  helper->Start();
  RunBackgroundTasks();
  ASSERT_TRUE(future.Get().has_value());
  VerifyResult(future.Get().value());
}

TEST_F(RegistrationTokenHelperTest, Failure) {
  // Emulates key generation failure.
  crypto::ScopedNullUnexportableKeyProvider scoped_null_key_provider_;
  base::test::TestFuture<absl::optional<RegistrationTokenHelper::Result>>
      future;
  std::unique_ptr<RegistrationTokenHelper> helper =
      RegistrationTokenHelper::CreateForTokenBinding(
          unexportable_key_service(), "test_client_id", "test_auth_code",
          GURL("https://accounts.google.com/Register"), future.GetCallback());
  helper->Start();
  RunBackgroundTasks();
  EXPECT_FALSE(future.Get().has_value());
}
