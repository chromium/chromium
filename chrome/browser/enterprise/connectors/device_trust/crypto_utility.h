// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_CRYPTO_UTILITY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_CRYPTO_UTILITY_H_

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_attestation_ca.pb.h"

namespace enterprise_connector {

// A class which provides helpers for cryptography-related tasks.
class CryptoUtility {
 public:
  // Verifies a PKCS #1 v1.5 SHA-256 |signature| over |data| with digest
  // algorithm |digest_nid|. The |public_key_hex| contains a modulus in hex
  // format.
  static bool VerifySignatureUsingHexKey(
      const std::string& public_key_modulus_hex,
      const std::string& data,
      const std::string& signature);
};

}  // namespace enterprise_connector

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_CRYPTO_UTILITY_H_
