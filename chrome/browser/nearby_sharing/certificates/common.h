// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_COMMON_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_COMMON_H_

#include <array>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"

// Returns true if the |current_time| exceeds |not_after| by more than the
// public certificate clock-skew tolerance if applicable.
bool IsNearbyShareCertificateExpired(base::Time current_time,
                                     base::Time not_after,
                                     bool use_public_certificate_tolerance);

// Returns true if the |current_time| is in the interval
// [|not_before| - tolerance, |not_after| + tolerance), where a clock-skew
// tolerance is only non-zero if |use_public_certificate_tolerance| is true.
bool IsNearbyShareCertificateWithinValidityPeriod(
    base::Time current_time,
    base::Time not_before,
    base::Time not_after,
    bool use_public_certificate_tolerance);

// Uses HKDF to create a hash of the |authentication_token|, using the
// |secret_key|. A trivial info parameter is used, and the output length is
// fixed to be kNearbyShareNumBytesAuthenticationTokenHash to conform with the
// GmsCore implementation.
std::array<uint8_t, kNearbyShareNumBytesAuthenticationTokenHash>
ComputeAuthenticationTokenHash(base::span<const uint8_t> authentication_token,
                               base::span<const uint8_t> secret_key);

// Uses HKDF to generate a new key of length |NewNumBytes| from |key|. To
// conform with the GmsCore implementation, trivial salt and info are used.
template <size_t NewNumBytes>
std::array<uint8_t, NewNumBytes> DeriveNearbyShareKey(
    base::span<const uint8_t> key) {
  return crypto::HkdfSha256<NewNumBytes>(key, /*salt=*/{}, /*info=*/{});
}

// Generates a random byte array with size |num_bytes|.
template <size_t NumBytes>
std::array<uint8_t, NumBytes> GenerateRandomBytes() {
  std::array<uint8_t, NumBytes> bytes;
  crypto::RandBytes(bytes);
  return bytes;
}

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_COMMON_H_
