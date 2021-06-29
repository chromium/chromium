// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_ASH_H_

#include <string>
#include <vector>

#include "chrome/browser/platform_keys/platform_keys.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace extensions {
namespace platform_keys {

// Converts a token id from ::chromeos::platform_keys to the platformKeys API
// token id.
std::string PlatformKeysTokenIdToApiId(
    chromeos::platform_keys::TokenId platform_keys_token_id);

}  // namespace platform_keys

class PlatformKeysVerifyTLSServerCertificateFunction
    : public ExtensionFunction {
 private:
  ~PlatformKeysVerifyTLSServerCertificateFunction() override;
  ResponseAction Run() override;

  void FinishedVerification(const std::string& error,
                            int verify_result,
                            int cert_status);

  DECLARE_EXTENSION_FUNCTION("platformKeys.verifyTLSServerCertificate",
                             PLATFORMKEYS_VERIFYTLSSERVERCERTIFICATE)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_ASH_H_
