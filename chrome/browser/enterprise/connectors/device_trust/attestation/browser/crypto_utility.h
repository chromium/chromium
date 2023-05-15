// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_CRYPTO_UTILITY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_CRYPTO_UTILITY_H_

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace enterprise_connectors {

namespace CryptoUtility {

// Verifies a PKCS #1 v1.5 SHA-256 |signature| over |data| with digest
// algorithm |digest_nid|. The |public_key_hex| contains a modulus in hex
// format.
bool VerifySignatureUsingHexKey(const std::string& public_key_modulus_hex,
                                const std::string& data,
                                const std::string& signature);

bool CreatePubKeyFromHex(const std::string& public_key_modulus_hex,
                         std::vector<uint8_t>& public_key_info);

// Encrypts |input| using AES-256-CBC-PKCS5, a random IV, and HMAC-SHA512 over
// the cipher-text. The encryption and mac keys are derived from a random
// symmetric key. On success populates |key| and |output| and returns true. The
// output.wrapped_key and output.wrapping_key_id fields are ignored.
bool EncryptWithSeed(const std::string& data,
                     EncryptedData* encrypted,
                     std::string& key);
// MAC: HMAC-SHA-512 using the AES key.
std::string HmacSha512(const std::string& key, const std::string& data);

// Wraps |key| with |wrapping_key| using RSA-PKCS1-OAEP. On success populates
// output.wrapped_key and output.wrapping_key_id fields (other fields are
// ignored).
bool WrapKeyOAEP(const std::string& key,
                 RSA* wrapping_key,
                 const std::string& wrapping_key_id,
                 EncryptedData* output);

bssl::UniquePtr<RSA> GetRSA(const std::string& public_key_modulus_hex);

}  // namespace CryptoUtility

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_CRYPTO_UTILITY_H_
