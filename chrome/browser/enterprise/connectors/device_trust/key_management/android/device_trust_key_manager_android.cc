// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/android/device_trust_key_manager_android.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/android_attestation_token_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "crypto/hash.h"

namespace enterprise_connectors {

namespace {

constexpr char kSuccessLatencyHistogram[] =
    "Enterprise.DeviceTrust.AttestationToken.Latency.Success";
constexpr char kFailureLatencyHistogram[] =
    "Enterprise.DeviceTrust.AttestationToken.Latency.Failure";
constexpr char kTokenSizeHistogram[] =
    "Enterprise.DeviceTrust.AttestationToken.Size";

// Records token metrics, then forwards `token` to `callback`.
void OnTokenGenerated(
    base::TimeTicks start_time,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback,
    std::optional<std::vector<uint8_t>> token) {
  // Medium times: generation can exceed the 10 s `UmaHistogramTimes()` cap.
  base::UmaHistogramMediumTimes(
      token ? kSuccessLatencyHistogram : kFailureLatencyHistogram,
      base::TimeTicks::Now() - start_time);

  if (token) {
    base::UmaHistogramCounts100000(kTokenSizeHistogram,
                                   base::saturated_cast<int>(token->size()));
  }

  std::move(callback).Run(std::move(token));
}

}  // namespace

DeviceTrustKeyManagerAndroid::DeviceTrustKeyManagerAndroid(
    std::unique_ptr<AndroidAttestationTokenClient> client,
    PrefService* local_state)
    : client_(std::move(client)), local_state_(CHECK_DEREF(local_state)) {
  CHECK(client_);
}

DeviceTrustKeyManagerAndroid::~DeviceTrustKeyManagerAndroid() = default;

void DeviceTrustKeyManagerAndroid::StartInitialization() {
  if (!IsDeviceTrustAndroidAttestationTokensEnabled()) {
    return;
  }

  const base::Time now = base::Time::Now();
  const base::Time last_request_time =
      local_state_->GetTime(kDeviceTrustAttestationTokenLastRequestTimePref);
  // An unset pref reads as a null `base::Time`, so the first call pre-warms.
  if (now - last_request_time < kPreWarmInterval) {
    return;
  }

  // `PreWarmCache()` doesn't report its result, so a failed pre-warm is only
  // retried after `kPreWarmInterval`.
  local_state_->SetTime(kDeviceTrustAttestationTokenLastRequestTimePref, now);
  LOG_POLICY(INFO, DEVICE_TRUST)
      << "Pre-warming the attestation token provider.";
  client_->PreWarmCache();
}

// Key rotation is not supported on Android. Post a task to fulfill the
// interface contract and ensure the callback is never invoked synchronously.
void DeviceTrustKeyManagerAndroid::RotateKey(
    const std::string& /*nonce*/,
    base::OnceCallback<void(KeyRotationResult)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), KeyRotationResult::FAILURE));
}

// Post a task to fulfill the interface contract and guarantee the callback is
// never called synchronously, preventing re-entrancy.
void DeviceTrustKeyManagerAndroid::ExportPublicKeyAsync(
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void DeviceTrustKeyManagerAndroid::SignStringAsync(
    const std::string& str,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback) {
  if (!IsDeviceTrustAndroidAttestationTokensEnabled()) {
    // Post a task with `nullopt` to produce an unsigned response
    // (`kSuccessNoSignature`) while fulfilling the asynchronous interface
    // contract and ensuring the callback is never invoked synchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // A token request warms up the attestation token provider's cache as much as
  // a pre-warm does, so it postpones the next pre-warm.
  local_state_->SetTime(kDeviceTrustAttestationTokenLastRequestTimePref,
                        base::Time::Now());

  // Bind the attestation token to the content of the serialized challenge
  // response.
  client_->GenerateToken(
      crypto::hash::Sha256(str),
      // Not bound to `this`: `callback` must run even if `this` is destroyed.
      base::BindOnce(&OnTokenGenerated, base::TimeTicks::Now(),
                     std::move(callback)));
}

std::optional<DeviceTrustKeyManager::KeyMetadata>
DeviceTrustKeyManagerAndroid::GetLoadedKeyMetadata() const {
  return std::nullopt;
}

bool DeviceTrustKeyManagerAndroid::HasPermanentFailure() const {
  return false;
}

}  // namespace enterprise_connectors
