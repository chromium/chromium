// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/android/device_trust_key_manager_android.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/android_attestation_token_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAreArray;

namespace enterprise_connectors {

namespace {

using KeyRotationResult = DeviceTrustKeyManager::KeyRotationResult;
using TokenCallback =
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>;

constexpr char kTestPayload[] = "test_challenge_payload";

class MockAndroidAttestationTokenClient : public AndroidAttestationTokenClient {
 public:
  MockAndroidAttestationTokenClient() = default;
  ~MockAndroidAttestationTokenClient() override = default;

  MOCK_METHOD(void,
              GenerateToken,
              (base::span<const uint8_t> content_binding_hash,
               TokenCallback callback),
              (override));
  MOCK_METHOD(void, PreWarmCache, (), (override));
};

}  // namespace

class DeviceTrustKeyManagerAndroidTest : public testing::Test {
 protected:
  // Creates the key manager. Must be called after the feature state is set.
  void CreateKeyManager() {
    auto mock_client = std::make_unique<MockAndroidAttestationTokenClient>();
    mock_client_ = mock_client.get();
    key_manager_ =
        std::make_unique<DeviceTrustKeyManagerAndroid>(std::move(mock_client));
  }

  // Declared before `task_environment_` so that it outlives it: the global
  // FeatureList must not be torn down while other threads may still read it.
  base::test::ScopedFeatureList scoped_feature_list_;
  // No thread pool is needed, since the token client is mocked.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<DeviceTrustKeyManagerAndroid> key_manager_;
  raw_ptr<MockAndroidAttestationTokenClient> mock_client_ = nullptr;
};

class DeviceTrustKeyManagerAndroidFeatureDisabledTest
    : public DeviceTrustKeyManagerAndroidTest {
 protected:
  DeviceTrustKeyManagerAndroidFeatureDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kDeviceTrustAndroidAttestationTokens);
    CreateKeyManager();
  }
};

class DeviceTrustKeyManagerAndroidFeatureEnabledTest
    : public DeviceTrustKeyManagerAndroidTest {
 protected:
  DeviceTrustKeyManagerAndroidFeatureEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kDeviceTrustAndroidAttestationTokens);
    CreateKeyManager();
  }
};

// Tests that, when the feature is disabled, `SignStringAsync()` never calls
// the token client and completes asynchronously with `nullopt`, signaling to
// the attestation pipeline that an unsigned challenge response
// (`kSuccessNoSignature`) should be generated.
TEST_F(DeviceTrustKeyManagerAndroidFeatureDisabledTest,
       SignStringAsync_FeatureDisabled) {
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_->SignStringAsync(kTestPayload, future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `StartInitialization()` is a safe no-op on Android: it does not
// request a token and leaves the key manager with no loaded key metadata nor
// permanent failures.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest, StartInitialization) {
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);

  key_manager_->StartInitialization();

  EXPECT_FALSE(key_manager_->HasPermanentFailure());
  EXPECT_EQ(key_manager_->GetLoadedKeyMetadata(), std::nullopt);
}

// Tests that `RotateKey()` invokes its callback asynchronously with `FAILURE`
// since key rotation is not supported on Android.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       RotateKeyReturnsFailure) {
  base::test::TestFuture<KeyRotationResult> future;
  key_manager_->RotateKey("test_nonce", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), KeyRotationResult::FAILURE);
}

// Tests that `ExportPublicKeyAsync()` completes asynchronously and returns
// `nullopt` because no browser signing key is provisioned.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       ExportPublicKeyAsyncReturnsNullopt) {
  base::test::TestFuture<std::optional<std::string>> future;
  key_manager_->ExportPublicKeyAsync(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `HasPermanentFailure()` returns false by default.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       HasPermanentFailureReturnsFalse) {
  EXPECT_FALSE(key_manager_->HasPermanentFailure());
}

// Tests that, when the feature is enabled, `SignStringAsync()` requests a token
// bound to the SHA-256 hash of the payload and forwards the token as-is.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       SignStringAsync_FeatureEnabled) {
  const std::vector<uint8_t> kToken = {0x01, 0x02, 0x03};
  const auto expected_hash = crypto::hash::Sha256(kTestPayload);

  EXPECT_CALL(*mock_client_, GenerateToken(ElementsAreArray(expected_hash), _))
      .WillOnce([&kToken](base::span<const uint8_t>, TokenCallback callback) {
        std::move(callback).Run(kToken);
      });

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_->SignStringAsync(kTestPayload, future.GetCallback());
  EXPECT_EQ(future.Get(), kToken);
}

// Tests that a token generation failure is surfaced as `nullopt`.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       SignStringAsync_FeatureEnabled_TokenFailure) {
  EXPECT_CALL(
      *mock_client_,
      GenerateToken(ElementsAreArray(crypto::hash::Sha256(kTestPayload)), _))
      .WillOnce([](base::span<const uint8_t>, TokenCallback callback) {
        std::move(callback).Run(std::nullopt);
      });

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_->SignStringAsync(kTestPayload, future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);
}

}  // namespace enterprise_connectors
