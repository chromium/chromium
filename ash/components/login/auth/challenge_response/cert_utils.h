// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_
#define ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_

#include <cstdint>
#include <vector>

#include "ash/components/login/auth/public/challenge_response_key.h"
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace ash {

// Maps from the TLS 1.3 SignatureScheme value into the challenge-response key
// algorithm.
COMPONENT_EXPORT(ASH_LOGIN_AUTH)
absl::optional<ChallengeResponseKey::SignatureAlgorithm>
GetChallengeResponseKeyAlgorithmFromSsl(uint16_t ssl_algorithm);

// Constructs the ChallengeResponseKey instance based on the public key referred
// by the specified certificate and on the specified list of supported
// algorithms. Returns false on failure.
bool COMPONENT_EXPORT(ASH_LOGIN_AUTH) ExtractChallengeResponseKeyFromCert(
    const net::X509Certificate& certificate,
    const std::vector<ChallengeResponseKey::SignatureAlgorithm>&
        signature_algorithms,
    ChallengeResponseKey* challenge_response_key);

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_
