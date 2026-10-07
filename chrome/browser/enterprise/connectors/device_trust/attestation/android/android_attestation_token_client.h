// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ANDROID_ANDROID_ATTESTATION_TOKEN_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ANDROID_ANDROID_ATTESTATION_TOKEN_CLIENT_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"

namespace enterprise_connectors {

// Bridges native Device Trust code to the Java AttestationTokenGenerator via
// JNI. Methods are virtual to allow mocking in tests of higher-level classes.
class AndroidAttestationTokenClient {
 public:
  AndroidAttestationTokenClient();
  AndroidAttestationTokenClient(const AndroidAttestationTokenClient&) = delete;
  AndroidAttestationTokenClient& operator=(
      const AndroidAttestationTokenClient&) = delete;
  virtual ~AndroidAttestationTokenClient();

  // Generates an attestation token bound to `content_binding_hash`, which must
  // be a raw SHA-256 hash (32 bytes). The bytes
  // are copied, so `content_binding_hash` only needs to remain valid for the
  // duration of this call. The (potentially blocking) Java call runs on a
  // thread pool worker, and `callback` is invoked on the calling sequence with
  // the token, or std::nullopt on failure.
  //
  // The Java call cannot be cancelled, so the worker is held until it returns.
  // The Java delegate is required to return within a bounded time.
  virtual void GenerateToken(
      base::span<const uint8_t> content_binding_hash,
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback);

  // Asynchronously pre-warms the token provider's cache on a thread pool
  // worker. This method is to be used as a fire-and-forget.
  virtual void PreWarmCache();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ANDROID_ANDROID_ATTESTATION_TOKEN_CLIENT_H_
