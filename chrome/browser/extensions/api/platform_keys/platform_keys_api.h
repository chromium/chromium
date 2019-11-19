// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_H_

#include <string>
#include <vector>

#include "extensions/browser/extension_function.h"

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // net

namespace extensions {
namespace platform_keys {

extern const char kErrorInvalidToken[];
extern const char kErrorInvalidX509Cert[];

// Returns whether |token_id| references a known Token.
bool ValidateToken(const std::string& token_id,
                   std::string* platform_keys_token_id);

// Converts a token id from ::chromeos::platform_keys to the platformKeys API
// token id.
std::string PlatformKeysTokenIdToApiId(
    const std::string& platform_keys_token_id);

}  // namespace platform_keys

class PlatformKeysInternalSelectClientCertificatesFunction
    : public ExtensionFunction {
 private:
  ~PlatformKeysInternalSelectClientCertificatesFunction() override;
  ResponseAction Run() override;

  // Called when the certificates were selected. If an error occurred, |certs|
  // will be null and instead |error_message| be set.
  void OnSelectedCertificates(std::unique_ptr<net::CertificateList> matches,
                              const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("platformKeysInternal.selectClientCertificates",
                             PLATFORMKEYSINTERNAL_SELECTCLIENTCERTIFICATES)
};

class PlatformKeysInternalGetPublicKeyFunction : public ExtensionFunction {
 private:
  ~PlatformKeysInternalGetPublicKeyFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("platformKeysInternal.getPublicKey",
                             PLATFORMKEYSINTERNAL_GETPUBLICKEY)
};

class PlatformKeysInternalSignFunction : public ExtensionFunction {
 private:
  ~PlatformKeysInternalSignFunction() override;
  ResponseAction Run() override;

  // Called when the signature was generated. If an error occurred,
  // |signature| will be empty and instead |error_message| be set.
  void OnSigned(const std::string& signature, const std::string& error_message);

  DECLARE_EXTENSION_FUNCTION("platformKeysInternal.sign",
                             PLATFORMKEYSINTERNAL_SIGN)
};

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

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_API_H_
