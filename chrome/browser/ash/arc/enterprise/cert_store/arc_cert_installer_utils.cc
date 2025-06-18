// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer_utils.h"

#include <string_view>

#include "base/base64.h"
#include "base/logging.h"
#include "crypto/rsa_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

std::string CreatePkcs12ForKey(const std::string& name, EVP_PKEY* key) {
  if (!key) {
    VLOG(1) << "Failed due to a nullptr key";
    return "";
  }

  // Make a PKCS#12 blob. Specify obsolete encryption because ARC++ seems to
  // break with modern values. See crbug.com/420764006. However this is
  // encrypted with an empty password, so there is no security here anyway.
  // This should be switched to PKCS#8 PrivateKeyInfo if encryption is not
  // necessary.
  bssl::UniquePtr<PKCS12> pkcs12(
      PKCS12_create(nullptr, name.c_str(), key, nullptr, nullptr,
                    /*key_nid=*/NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
                    /*cert_nid=*/NID_pbe_WithSHA1And40BitRC2_CBC,
                    /*iterations=*/2048, /*mac_iterators=*/1, /*key_type=*/0));
  if (!pkcs12) {
    VLOG(1) << "Failed to create PKCS12 object from |key| for " << name;
    return "";
  }

  uint8_t* pkcs12_key = nullptr;
  int pkcs12_key_len;
  if (!(pkcs12_key_len = i2d_PKCS12(pkcs12.get(), &pkcs12_key))) {
    VLOG(1) << "Failed to translate PKCS12 to byte array for " << name;
    return "";
  }

  bssl::UniquePtr<uint8_t> free_pkcs12_key(pkcs12_key);
  return base::Base64Encode(
      std::string_view((char*)pkcs12_key, pkcs12_key_len));
}

std::string ExportSpki(crypto::RSAPrivateKey* rsa) {
  DCHECK(rsa);
  std::vector<uint8_t> spki;
  if (!rsa->ExportPublicKey(&spki)) {
    LOG(ERROR) << "Key export has failed.";
    return "";
  }
  return base::Base64Encode(spki);
}

}  // namespace arc
