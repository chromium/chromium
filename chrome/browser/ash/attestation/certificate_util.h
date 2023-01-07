// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_CERTIFICATE_UTIL_H_
#define CHROME_BROWSER_ASH_ATTESTATION_CERTIFICATE_UTIL_H_

#include <string>

#include "base/time/time.h"

namespace ash {
namespace attestation {

enum class CertificateExpiryStatus {
  kValid,
  kExpiringSoon,
  kExpired,
  kInvalidPemChain,
  kInvalidX509,
};

// Checks if |certificate_chain| is a PEM certificate chain that contains a
// certificate this is expired or expiring soon according to |expiry_threshold|.
// Returns the expiry status with the following precedence:
//  1. If there is an expired token in |certificate_chain|, returns kExpired.
//  2. If there is an expiring soon token but no expired token, returns
//     kExpiringSoon.
//  3. If there are no expired or expiring soon tokens but there is an invalid
//     token, returns kInvalidX509.
//  4. If there are no parsable tokens, returns kInvalidPemChain.
//  5. Otherwise, returns kValid.
CertificateExpiryStatus CheckCertificateExpiry(
    const std::string& certificate_chain,
    base::TimeDelta expiry_threshold);

std::string CertificateExpiryStatusToString(CertificateExpiryStatus status);

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_CERTIFICATE_UTIL_H_
