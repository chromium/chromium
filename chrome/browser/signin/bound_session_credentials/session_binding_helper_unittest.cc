// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"

#include <string>

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
using base::test::RunOnceCallback;
using testing::_;
using testing::Invoke;
using testing::Return;

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;
}  // namespace

class SessionBindingHelperTest : public testing::Test {
 public:
  SessionBindingHelperTest()
      : unexportable_key_service_(unexportable_key_task_manager_) {}

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  unexportable_keys::UnexportableKeyId GenerateNewKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
        generate_future;
    unexportable_key_service_.GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        key_id = generate_future.Get();
    CHECK(key_id.has_value());
    return *key_id;
  }

  std::vector<uint8_t> GetWrappedKey(
      const unexportable_keys::UnexportableKeyId& key_id) {
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        unexportable_key_service_.GetWrappedKey(key_id);
    CHECK(wrapped_key.has_value());
    return *wrapped_key;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
};

TEST_F(SessionBindingHelperTest, MaybeLoadBindingKey) {
  unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  SessionBindingHelper helper(unexportable_key_service(), GetWrappedKey(key_id),
                              "session_id");
  EXPECT_FALSE(helper.key_loader_);
  helper.MaybeLoadBindingKey();
  unexportable_keys::UnexportableKeyLoader* key_loader =
      helper.key_loader_.get();
  EXPECT_TRUE(key_loader);
  EXPECT_NE(key_loader->GetStateForTesting(),
            unexportable_keys::UnexportableKeyLoader::State::kNotStarted);
  base::test::TestFuture<
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
      key_future;
  key_loader->InvokeCallbackAfterKeyLoaded(key_future.GetCallback());
  EXPECT_EQ(*key_future.Get(), key_id);
}

TEST_F(SessionBindingHelperTest, GenerateBindingKeyAssertion) {
  unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  SessionBindingHelper helper(unexportable_key_service(), GetWrappedKey(key_id),
                              "session_id");
  base::test::TestFuture<
      base::expected<std::string, SessionBindingHelper::Error>>
      sign_future;
  helper.GenerateBindingKeyAssertion(
      "challenge", GURL("https://accounts.google.com/RotateBoundCookies"),
      sign_future.GetCallback());
  base::expected<std::string, SessionBindingHelper::Error> assertion_or_error =
      sign_future.Get();
  ASSERT_THAT(assertion_or_error, base::test::HasValue());

  EXPECT_TRUE(signin::VerifyJwtSignature(
      *assertion_or_error, *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
}

TEST_F(SessionBindingHelperTest, GenerateBindingKeyAssertionInvalidBindingKey) {
  const std::vector<uint8_t> kInvalidWrappedKey = {1, 2, 3};
  SessionBindingHelper helper(unexportable_key_service(), kInvalidWrappedKey,
                              "session_id");

  base::test::TestFuture<
      base::expected<std::string, SessionBindingHelper::Error>>
      sign_future;
  helper.GenerateBindingKeyAssertion(
      "challenge", GURL("https://accounts.google.com/RotateBoundCookies"),
      sign_future.GetCallback());
  base::expected<std::string, SessionBindingHelper::Error> assertion_or_error =
      sign_future.Get();
  EXPECT_THAT(assertion_or_error,
              base::test::ErrorIs(
                  testing::Eq(SessionBindingHelper::Error::kLoadKeyFailure)));
}

TEST_F(SessionBindingHelperTest, ReloadKeyAfterFailure) {
  const unexportable_keys::UnexportableKeyId key_id = GenerateNewKey();
  const std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id);
  // Put a mock key service in front of the real one to simulate errors.
  testing::StrictMock<unexportable_keys::MockUnexportableKeyService>
      mock_unexportable_key_service;
  EXPECT_CALL(mock_unexportable_key_service, GetAlgorithm(key_id))
      .WillRepeatedly(Return(unexportable_key_service().GetAlgorithm(key_id)));
  EXPECT_CALL(mock_unexportable_key_service, GetWrappedKey(key_id))
      .WillRepeatedly(Return(unexportable_key_service().GetWrappedKey(key_id)));
  EXPECT_CALL(mock_unexportable_key_service, GetSubjectPublicKeyInfo(key_id))
      .WillRepeatedly(
          Return(unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
  SessionBindingHelper helper(mock_unexportable_key_service, wrapped_key,
                              "session_id");
  {
    base::test::TestFuture<
        base::expected<std::string, SessionBindingHelper::Error>>
        sign_future;
    EXPECT_CALL(
        mock_unexportable_key_service,
        FromWrappedSigningKeySlowlyAsync(base::make_span(wrapped_key), _, _))
        .WillOnce(RunOnceCallback<2>(base::unexpected(
            unexportable_keys::ServiceError::kCryptoApiFailed)));
    helper.GenerateBindingKeyAssertion(
        "challenge", GURL("https://accounts.google.com/RotateBoundCookies"),
        sign_future.GetCallback());
    ASSERT_EQ(sign_future.Get(),
              base::unexpected(SessionBindingHelper::Error::kLoadKeyFailure));
  }

  {
    base::test::TestFuture<
        base::expected<std::string, SessionBindingHelper::Error>>
        sign_future;
    EXPECT_CALL(
        mock_unexportable_key_service,
        FromWrappedSigningKeySlowlyAsync(base::make_span(wrapped_key), _, _))
        .WillOnce(RunOnceCallback<2>(key_id));
    EXPECT_CALL(mock_unexportable_key_service, SignSlowlyAsync(key_id, _, _, _))
        .WillOnce(Invoke(
            &unexportable_key_service(),
            &unexportable_keys::UnexportableKeyService::SignSlowlyAsync));
    helper.GenerateBindingKeyAssertion(
        "challenge", GURL("https://accounts.google.com/RotateBoundCookies"),
        sign_future.GetCallback());
    // Verify that the key was loaded successfully.
    EXPECT_THAT(sign_future.Get(), base::test::HasValue());
  }
}
