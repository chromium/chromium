// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_HELPERS_KEY_HELPER_H_
#define ASH_COMPONENTS_KCER_HELPERS_KEY_HELPER_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace kcer::internal {

// Calculates and returns CKA_ID from public key bytes (`public_key_bytes`).
crypto::ScopedSECItem MakeIdFromPubKeyNss(
    const std::vector<uint8_t>& public_key_bytes);

// Converts ScopedSECItem `id` to vector<uint8_t>.
std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& id);

// Creates PKCS11 id for the key (`key_data`). Returns a new id as vector.
std::vector<uint8_t> MakePkcs11IdForEcKey(base::span<const uint8_t> key_data);

// Extracts public key from EC_KEY object and returns it as X9.62 uncompressed
// bytes (ASN.1 OCTET STRING as bytes).
COMPONENT_EXPORT(KCER)
std::vector<uint8_t> GetEcPublicKeyBytes(const EC_KEY* ec_key);

// DER-encode an ASN.1 OCTET STRING.
COMPONENT_EXPORT(KCER)
std::vector<uint8_t> DerEncodeAsn1OctetString(
    base::span<const uint8_t> asn1_octet_string);

// Extracts private key from the `ec_key` object and returns it as bytes.
// Leading zeros will be padded.
COMPONENT_EXPORT(KCER)
std::vector<uint8_t> GetEcPrivateKeyBytes(const EC_KEY* ec_key);

// Extract EC params from `ec_key` and returns them as a DER-encoding of an ANSI
// X9.62 Parameters value.
std::vector<uint8_t> GetEcParamsDer(const EC_KEY* ec_key);

// Verify that `key` has type of EVP_PKEY_EC.
bool IsKeyEcType(const bssl::UniquePtr<EVP_PKEY>& key);

// Verify that `key` has type of EVP_PKEY_RSA.
bool IsKeyRsaType(const bssl::UniquePtr<EVP_PKEY>& key);

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_HELPERS_KEY_HELPER_H_
