// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/common.h"

#include <array>

#include "base/logging.h"
#include "base/rand_util.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "crypto/hkdf.h"
#include "crypto/sha2.h"

bool IsNearbyShareCertificateExpired(base::Time current_time,
                                     base::Time not_after,
                                     bool use_public_certificate_tolerance) {
  base::TimeDelta tolerance =
      use_public_certificate_tolerance
          ? kNearbySharePublicCertificateValidityBoundOffsetTolerance
          : base::Seconds(0);

  return current_time >= not_after + tolerance;
}

bool IsNearbyShareCertificateWithinValidityPeriod(
    base::Time current_time,
    base::Time not_before,
    base::Time not_after,
    bool use_public_certificate_tolerance) {
  base::TimeDelta tolerance =
      use_public_certificate_tolerance
          ? kNearbySharePublicCertificateValidityBoundOffsetTolerance
          : base::Seconds(0);

  return current_time >= not_before - tolerance &&
         !IsNearbyShareCertificateExpired(current_time, not_after,
                                          use_public_certificate_tolerance);
}

std::array<uint8_t, kNearbyShareNumBytesAuthenticationTokenHash>
ComputeAuthenticationTokenHash(base::span<const uint8_t> authentication_token,
                               base::span<const uint8_t> secret_key) {
  return crypto::HkdfSha256<kNearbyShareNumBytesAuthenticationTokenHash>(
      authentication_token, secret_key,
      /*info=*/base::span<const uint8_t>());
}
