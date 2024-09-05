// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/kcer/helpers/key_helper.h"

#include <pk11pub.h>
#include <stdint.h>

#include <vector>

#include "base/hash/sha1.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/include/openssl/asn1.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace kcer::internal {

crypto::ScopedSECItem MakeIdFromPubKeyNss(
    const std::vector<uint8_t>& public_key_bytes) {
  SECItem secitem_modulus;
  secitem_modulus.data = const_cast<uint8_t*>(public_key_bytes.data());
  secitem_modulus.len = public_key_bytes.size();
  return crypto::ScopedSECItem(PK11_MakeIDFromPubKey(&secitem_modulus));
}

std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& id) {
  if (!id || id->len == 0) {
    return {};
  }
  return std::vector<uint8_t>(id->data, id->data + id->len);
}

std::vector<uint8_t> MakePkcs11IdForEcKey(base::span<const uint8_t> key_data) {
  if (key_data.size() <= base::kSHA1Length) {
    return std::vector<uint8_t>(key_data.begin(), key_data.end());
  }

  base::SHA1Digest hash = base::SHA1Hash(key_data);
  return std::vector<uint8_t>(hash.begin(), hash.end());
}

std::vector<uint8_t> GetEcPublicKeyBytes(const EC_KEY* ec_key) {
  if (!ec_key) {
    return {};
  }
  const EC_POINT* point = EC_KEY_get0_public_key(ec_key);
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);

  if (!point || !group) {
    return {};
  }

  size_t point_len =
      EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
                         /*buf=*/nullptr, /*max_out=*/0, /*ctx=*/nullptr);
  if (point_len == 0) {
    return {};
  }

  std::vector<uint8_t> buf(point_len);
  if (EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
                         buf.data(), buf.size(),
                         /*ctx=*/nullptr) != buf.size()) {
    return {};
  }
  return buf;
}

std::vector<uint8_t> DerEncodeAsn1OctetString(
    base::span<const uint8_t> asn1_octet_string) {
  if (asn1_octet_string.empty()) {
    return {};
  }

  bssl::UniquePtr<ASN1_OCTET_STRING> octet_string(ASN1_OCTET_STRING_new());
  if (!ASN1_OCTET_STRING_set(octet_string.get(), asn1_octet_string.data(),
                             asn1_octet_string.size())) {
    return {};
  }

  int der_size = i2d_ASN1_OCTET_STRING(octet_string.get(), nullptr);
  if (der_size <= 0) {
    return {};
  }

  std::vector<uint8_t> result_der(der_size);
  uint8_t* output_buffer = result_der.data();
  if (i2d_ASN1_OCTET_STRING(octet_string.get(), &output_buffer) <= 0) {
    return {};
  }
  return result_der;
}

std::vector<uint8_t> GetEcPrivateKeyBytes(const EC_KEY* ec_key) {
  if (!ec_key) {
    return {};
  }
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);
  const BIGNUM* priv_key = EC_KEY_get0_private_key(ec_key);
  if (!priv_key || !group) {
    return {};
  }
  size_t priv_key_size_bits = EC_GROUP_order_bits(group);
  size_t priv_key_bytes = (priv_key_size_bits + 7) / 8;
  std::vector<uint8_t> buffer(priv_key_bytes);
  int extract_result =
      BN_bn2bin_padded(buffer.data(), priv_key_bytes, priv_key);

  if (!extract_result) {
    return {};
  }
  return buffer;
}

std::vector<uint8_t> GetEcParamsDer(const EC_KEY* ec_key) {
  bssl::ScopedCBB cbb;
  uint8_t* ec_params_der = nullptr;
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);
  size_t ec_params_der_len = 0;
  if (!CBB_init(cbb.get(), 0) || !EC_KEY_marshal_curve_name(cbb.get(), group) ||
      !CBB_finish(cbb.get(), &ec_params_der, &ec_params_der_len)) {
    return {};
  }
  bssl::UniquePtr<uint8_t> der_deleter(ec_params_der);
  return std::vector<uint8_t>(ec_params_der, ec_params_der + ec_params_der_len);
}

bool IsKeyEcType(const bssl::UniquePtr<EVP_PKEY>& key) {
  return EVP_PKEY_base_id(key.get()) == EVP_PKEY_EC;
}

bool IsKeyRsaType(const bssl::UniquePtr<EVP_PKEY>& key) {
  return EVP_PKEY_base_id(key.get()) == EVP_PKEY_RSA;
}

}  // namespace kcer::internal
