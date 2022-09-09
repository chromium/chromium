// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_UTILS_H_

#include <string>

#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto {

class RSAPrivateKey;

}  // namespace crypto

namespace arc {

// Creates a PKCS12 container named |name| with private key |key|.
// Returns empty string in case of any error.
std::string CreatePkcs12ForKey(const std::string& name, EVP_PKEY* key);

// Exports the subject public key info of the given |rsa| key encoded in base64.
std::string ExportSpki(crypto::RSAPrivateKey* rsa);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_UTILS_H_
