// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
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
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_;
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
  base::test::TestFuture<std::string> sign_future;
  helper.GenerateBindingKeyAssertion(
      "challenge", GURL("https://accounts.google.com/RotateBoundCookies"),
      sign_future.GetCallback());
  std::string assertion = sign_future.Get();
  EXPECT_FALSE(assertion.empty());

  EXPECT_TRUE(signin::VerifyJwtSignature(
      assertion, *unexportable_key_service().GetAlgorithm(key_id),
      *unexportable_key_service().GetSubjectPublicKeyInfo(key_id)));
}

TEST_F(SessionBindingHelperTest, GenerateBindingKeyAssertionInvalidBindingKey) {
  const std::vector<uint8_t> kInvalidWrappedKey = {1, 2, 3};
  SessionBindingHelper helper(unexportable_key_service(), kInvalidWrappedKey,
                              "session_id");

  base::test::TestFuture<std::string> sign_future;
  helper.GenerateBindingKeyAssertion(
      "challenge", GURL("https://accounts.google.com/RotateBoundCookies"),
      sign_future.GetCallback());
  std::string assertion = sign_future.Get();
  EXPECT_TRUE(assertion.empty());
}
