// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/android/device_trust_key_manager_android.h"

#include <optional>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using KeyRotationResult = DeviceTrustKeyManager::KeyRotationResult;

}  // namespace

class DeviceTrustKeyManagerAndroidTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  DeviceTrustKeyManagerAndroid key_manager_;
};

// Tests that `StartInitialization()` is a safe no-op on Android and leaves the
// key manager with no loaded key metadata nor permanent failures.
TEST_F(DeviceTrustKeyManagerAndroidTest, StartInitialization) {
  key_manager_.StartInitialization();
  EXPECT_FALSE(key_manager_.HasPermanentFailure());
  EXPECT_EQ(key_manager_.GetLoadedKeyMetadata(), std::nullopt);
}

// Tests that `RotateKey()` invokes its callback asynchronously with `FAILURE`
// since key rotation is not supported on Android.
TEST_F(DeviceTrustKeyManagerAndroidTest, RotateKeyReturnsFailure) {
  base::test::TestFuture<KeyRotationResult> future;
  key_manager_.RotateKey("test_nonce", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), KeyRotationResult::FAILURE);
}

// Tests that `ExportPublicKeyAsync()` completes asynchronously and returns
// `nullopt` because no browser signing key is provisioned.
TEST_F(DeviceTrustKeyManagerAndroidTest, ExportPublicKeyAsyncReturnsNullopt) {
  base::test::TestFuture<std::optional<std::string>> future;
  key_manager_.ExportPublicKeyAsync(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `SignStringAsync()` completes asynchronously with `nullopt`,
// signaling to the attestation pipeline that an unsigned challenge response
// (`kSuccessNoSignature`) should be generated.
TEST_F(DeviceTrustKeyManagerAndroidTest,
       SignStringAsyncReturnsNulloptForUnsignedResponse) {
  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_.SignStringAsync("test_challenge_payload", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `HasPermanentFailure()` returns false by default.
TEST_F(DeviceTrustKeyManagerAndroidTest, HasPermanentFailureReturnsFalse) {
  EXPECT_FALSE(key_manager_.HasPermanentFailure());
}

}  // namespace enterprise_connectors
