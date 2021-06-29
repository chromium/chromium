// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_LACROS_H_

#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class PlatformKeysVerifyTLSServerCertificateFunction
    : public ExtensionFunction {
 private:
  ~PlatformKeysVerifyTLSServerCertificateFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("platformKeys.verifyTLSServerCertificate",
                             PLATFORMKEYS_VERIFYTLSSERVERCERTIFICATE)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_LACROS_H_
