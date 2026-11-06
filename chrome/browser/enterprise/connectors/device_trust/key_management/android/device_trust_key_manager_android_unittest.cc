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
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/android_attestation_token_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/prefs/testing_pref_service.h"
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

constexpr char kSuccessLatencyHistogram[] =
    "Enterprise.DeviceTrust.AttestationToken.Latency.Success";
constexpr char kFailureLatencyHistogram[] =
    "Enterprise.DeviceTrust.AttestationToken.Latency.Failure";
constexpr char kTokenSizeHistogram[] =
    "Enterprise.DeviceTrust.AttestationToken.Size";

// How long the mocked token client takes to return a result.
constexpr base::TimeDelta kTokenGenerationTime = base::Milliseconds(800);

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
  DeviceTrustKeyManagerAndroidTest() {
    RegisterDeviceTrustConnectorLocalStatePrefs(local_state_.registry());
  }

  // Creates the key manager. Must be called after the feature state is set.
  void CreateKeyManager() {
    auto mock_client = std::make_unique<MockAndroidAttestationTokenClient>();
    mock_client_ = mock_client.get();
    key_manager_ = std::make_unique<DeviceTrustKeyManagerAndroid>(
        std::move(mock_client), &local_state_);
  }

  base::Time GetLastRequestTime() const {
    return local_state_.GetTime(
        kDeviceTrustAttestationTokenLastRequestTimePref);
  }

  void SetLastRequestTime(base::Time time) {
    local_state_.SetTime(kDeviceTrustAttestationTokenLastRequestTimePref, time);
  }

  bool HasLastRequestTime() const {
    return local_state_.HasPrefPath(
        kDeviceTrustAttestationTokenLastRequestTimePref);
  }

  // Declared before `task_environment_` so that it outlives it: the global
  // FeatureList must not be torn down while other threads may still read it.
  base::test::ScopedFeatureList scoped_feature_list_;
  // No thread pool is needed, since the token client is mocked. Mock time lets
  // tests move the clock relative to the pre-warm interval and controls token
  // generation latency.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Declared before `key_manager_`, which keeps a pointer to it.
  TestingPrefServiceSimple local_state_;
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
// the token client nor records a token request, and completes asynchronously
// with `nullopt`, signaling to the attestation pipeline that an unsigned
// challenge response (`kSuccessNoSignature`) should be generated. No metrics
// are recorded.
TEST_F(DeviceTrustKeyManagerAndroidFeatureDisabledTest,
       SignStringAsync_FeatureDisabled) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_->SignStringAsync(kTestPayload, future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
  EXPECT_FALSE(HasLastRequestTime());

  histogram_tester.ExpectTotalCount(kSuccessLatencyHistogram, 0);
  histogram_tester.ExpectTotalCount(kFailureLatencyHistogram, 0);
  histogram_tester.ExpectTotalCount(kTokenSizeHistogram, 0);
}

// Tests that, when the feature is disabled, `StartInitialization()` neither
// pre-warms the attestation token provider nor records a token request.
TEST_F(DeviceTrustKeyManagerAndroidFeatureDisabledTest,
       StartInitialization_FeatureDisabled) {
  EXPECT_CALL(*mock_client_, PreWarmCache()).Times(0);
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);

  key_manager_->StartInitialization();

  EXPECT_FALSE(HasLastRequestTime());
}

// Tests that `StartInitialization()` pre-warms the attestation token provider
// without requesting a token, records the request time, and leaves the key
// manager with no loaded key metadata.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       StartInitialization_PreWarms) {
  EXPECT_CALL(*mock_client_, PreWarmCache());
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);

  key_manager_->StartInitialization();

  EXPECT_EQ(GetLastRequestTime(), base::Time::Now());
  EXPECT_EQ(key_manager_->GetLoadedKeyMetadata(), std::nullopt);
}

// Tests that `StartInitialization()` doesn't pre-warm again before
// `kPreWarmInterval` has passed since the previous pre-warm.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       StartInitialization_RateLimited) {
  EXPECT_CALL(*mock_client_, PreWarmCache());
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);
  key_manager_->StartInitialization();
  const base::Time pre_warm_time = base::Time::Now();

  task_environment_.FastForwardBy(
      DeviceTrustKeyManagerAndroid::kPreWarmInterval - base::Seconds(1));
  key_manager_->StartInitialization();

  EXPECT_EQ(GetLastRequestTime(), pre_warm_time);
}

// Tests that `StartInitialization()` pre-warms again once `kPreWarmInterval`
// has passed since the previous pre-warm.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       StartInitialization_PreWarmsAgainAfterInterval) {
  EXPECT_CALL(*mock_client_, PreWarmCache()).Times(2);
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);
  key_manager_->StartInitialization();

  task_environment_.FastForwardBy(
      DeviceTrustKeyManagerAndroid::kPreWarmInterval);
  key_manager_->StartInitialization();

  EXPECT_EQ(GetLastRequestTime(), base::Time::Now());
}

// Tests that the rate limit uses the request time stored in Local State, so
// that it also covers requests made in a previous browser session.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       StartInitialization_RateLimitedByStoredRequestTime) {
  const base::Time last_request_time = base::Time::Now() - base::Hours(1);
  SetLastRequestTime(last_request_time);
  CreateKeyManager();
  EXPECT_CALL(*mock_client_, PreWarmCache()).Times(0);
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);

  key_manager_->StartInitialization();

  EXPECT_EQ(GetLastRequestTime(), last_request_time);
}

// Tests that a stored request time in the future (e.g. after a clock change)
// also rate limits pre-warming, however far ahead it is, and is kept as is.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       StartInitialization_RateLimitedIfStoredRequestTimeIsInTheFuture) {
  const base::Time last_request_time = base::Time::Now() + base::Days(365);
  SetLastRequestTime(last_request_time);
  EXPECT_CALL(*mock_client_, PreWarmCache()).Times(0);
  EXPECT_CALL(*mock_client_, GenerateToken(_, _)).Times(0);

  key_manager_->StartInitialization();

  EXPECT_EQ(GetLastRequestTime(), last_request_time);
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
// bound to the SHA-256 hash of the payload, forwards it as-is and records its
// metrics.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       SignStringAsync_FeatureEnabled) {
  base::HistogramTester histogram_tester;
  const std::vector<uint8_t> kToken = {0x01, 0x02, 0x03};
  const auto expected_hash = crypto::hash::Sha256(kTestPayload);

  TokenCallback token_callback;
  EXPECT_CALL(*mock_client_, GenerateToken(ElementsAreArray(expected_hash), _))
      .WillOnce(MoveArg<1>(&token_callback));

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_->SignStringAsync(kTestPayload, future.GetCallback());

  task_environment_.FastForwardBy(kTokenGenerationTime);
  ASSERT_TRUE(token_callback);
  std::move(token_callback).Run(kToken);

  EXPECT_EQ(future.Get(), kToken);
  histogram_tester.ExpectUniqueTimeSample(kSuccessLatencyHistogram,
                                          kTokenGenerationTime, 1);
  histogram_tester.ExpectTotalCount(kFailureLatencyHistogram, 0);
  histogram_tester.ExpectUniqueSample(kTokenSizeHistogram, kToken.size(), 1);
}

// Tests that a failed token is forwarded as `nullopt` and recorded as failure.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       SignStringAsync_FeatureEnabled_TokenFailure) {
  base::HistogramTester histogram_tester;
  TokenCallback token_callback;
  EXPECT_CALL(
      *mock_client_,
      GenerateToken(ElementsAreArray(crypto::hash::Sha256(kTestPayload)), _))
      .WillOnce(MoveArg<1>(&token_callback));

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_->SignStringAsync(kTestPayload, future.GetCallback());

  task_environment_.FastForwardBy(kTokenGenerationTime);
  ASSERT_TRUE(token_callback);
  std::move(token_callback).Run(std::nullopt);

  EXPECT_EQ(future.Get(), std::nullopt);
  histogram_tester.ExpectUniqueTimeSample(kFailureLatencyHistogram,
                                          kTokenGenerationTime, 1);
  histogram_tester.ExpectTotalCount(kSuccessLatencyHistogram, 0);
  histogram_tester.ExpectTotalCount(kTokenSizeHistogram, 0);
}

// Tests that concurrent requests are timed independently. Both latencies exceed
// the 10 s `UmaHistogramTimes()` cap, so they only land in separate buckets
// with a longer range.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       SignStringAsync_FeatureEnabled_OverlappingRequests) {
  base::HistogramTester histogram_tester;
  const std::vector<uint8_t> kTokenA = {0x0a};
  const std::vector<uint8_t> kTokenB = {0x0b, 0x0b};

  TokenCallback token_callback_a;
  TokenCallback token_callback_b;
  EXPECT_CALL(*mock_client_, GenerateToken(_, _))
      .WillOnce(MoveArg<1>(&token_callback_a))
      .WillOnce(MoveArg<1>(&token_callback_b));

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future_a;
  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future_b;
  key_manager_->SignStringAsync(kTestPayload, future_a.GetCallback());
  task_environment_.FastForwardBy(base::Seconds(30));
  key_manager_->SignStringAsync(kTestPayload, future_b.GetCallback());

  // B completes after 20 s, then A after 50 s.
  task_environment_.FastForwardBy(base::Seconds(20));
  ASSERT_TRUE(token_callback_a);
  ASSERT_TRUE(token_callback_b);
  std::move(token_callback_b).Run(kTokenB);
  std::move(token_callback_a).Run(kTokenA);

  EXPECT_EQ(future_a.Get(), kTokenA);
  EXPECT_EQ(future_b.Get(), kTokenB);
  histogram_tester.ExpectTotalCount(kSuccessLatencyHistogram, 2);
  histogram_tester.ExpectTimeBucketCount(kSuccessLatencyHistogram,
                                         base::Seconds(20), 1);
  histogram_tester.ExpectTimeBucketCount(kSuccessLatencyHistogram,
                                         base::Seconds(50), 1);
  histogram_tester.ExpectBucketCount(kTokenSizeHistogram, kTokenA.size(), 1);
  histogram_tester.ExpectBucketCount(kTokenSizeHistogram, kTokenB.size(), 1);
}

// Tests that the token is forwarded and recorded even if the key manager is
// destroyed while the token is pending.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       SignStringAsync_FeatureEnabled_KeyManagerDestroyedWhilePending) {
  base::HistogramTester histogram_tester;
  const std::vector<uint8_t> kToken = {0x01, 0x02, 0x03};

  TokenCallback token_callback;
  EXPECT_CALL(*mock_client_, GenerateToken(_, _))
      .WillOnce(MoveArg<1>(&token_callback));

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_->SignStringAsync(kTestPayload, future.GetCallback());

  // Destroying the key manager also destroys the mocked client.
  mock_client_ = nullptr;
  key_manager_.reset();

  task_environment_.FastForwardBy(kTokenGenerationTime);
  ASSERT_TRUE(token_callback);
  std::move(token_callback).Run(kToken);

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), kToken);
  histogram_tester.ExpectUniqueTimeSample(kSuccessLatencyHistogram,
                                          kTokenGenerationTime, 1);
  histogram_tester.ExpectUniqueSample(kTokenSizeHistogram, kToken.size(), 1);
}

// Tests that requesting a token records the request time, which postpones the
// next pre-warm.
TEST_F(DeviceTrustKeyManagerAndroidFeatureEnabledTest,
       SignStringAsync_PostponesPreWarm) {
  EXPECT_CALL(*mock_client_, GenerateToken(_, _));
  EXPECT_CALL(*mock_client_, PreWarmCache()).Times(0);

  key_manager_->SignStringAsync(kTestPayload, base::DoNothing());
  EXPECT_EQ(GetLastRequestTime(), base::Time::Now());

  key_manager_->StartInitialization();
}

}  // namespace enterprise_connectors
