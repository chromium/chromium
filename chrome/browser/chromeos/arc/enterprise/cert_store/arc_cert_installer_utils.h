// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_UTILS_H_

#include <string>

#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace arc {

// Creates a PKCS12 container with RSA private key, generated with p = |blob|,
// to be able to extract |blob| value from a private key material later.
// Returns empty string in case of any error.
std::string CreatePkcs12FromBlob(const std::string& blob);

// Helper function that creates the RSA private key with p = |blob| to
// be able to extract |blob| value from a private key material later.
// Returns nullptr in case of any error.
// Should be used only for testing.
RSA* CreateRsaPrivateKeyFromBlob(const std::string& blob);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_CERT_STORE_ARC_CERT_INSTALLER_UTILS_H_
