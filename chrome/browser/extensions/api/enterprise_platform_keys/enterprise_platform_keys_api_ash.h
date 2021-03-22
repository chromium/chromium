// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_ASH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"
#include "extensions/browser/extension_function.h"

namespace net {
class X509Certificate;
using CertificateList = std::vector<scoped_refptr<X509Certificate>>;
}  // namespace net

namespace extensions {

class EnterprisePlatformKeysInternalGenerateKeyFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysInternalGenerateKeyFunction() override;
  ResponseAction Run() override;

  // Called when the key was generated. If an error occurred, |public_key_der|
  // will be empty.
  void OnGeneratedKey(const std::string& public_key_der,
                      chromeos::platform_keys::Status status);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.generateKey",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GENERATEKEY)
};

class EnterprisePlatformKeysGetCertificatesFunction : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysGetCertificatesFunction() override;
  ResponseAction Run() override;

  // Called when the list of certificates was determined. If an error occurred,
  // |certs| will be nullptr.
  void OnGotCertificates(std::unique_ptr<net::CertificateList> certs,
                         chromeos::platform_keys::Status status);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.getCertificates",
                             ENTERPRISE_PLATFORMKEYS_GETCERTIFICATES)
};

class EnterprisePlatformKeysImportCertificateFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysImportCertificateFunction() override;
  ResponseAction Run() override;

  // Called when the certificate was imported.
  void OnImportedCertificate(chromeos::platform_keys::Status status);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.importCertificate",
                             ENTERPRISE_PLATFORMKEYS_IMPORTCERTIFICATE)
};

class EnterprisePlatformKeysRemoveCertificateFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysRemoveCertificateFunction() override;
  ResponseAction Run() override;

  // Called when the certificate was removed.
  void OnRemovedCertificate(chromeos::platform_keys::Status status);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.removeCertificate",
                             ENTERPRISE_PLATFORMKEYS_REMOVECERTIFICATE)
};

class EnterprisePlatformKeysInternalGetTokensFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysInternalGetTokensFunction() override;
  ResponseAction Run() override;

  // Called when the list of tokens was determined. If an error occurred,
  // |token_ids| will be nullptr.
  void OnGotTokens(
      std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>> token_ids,
      chromeos::platform_keys::Status status);

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.getTokens",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GETTOKENS)
};

class EnterprisePlatformKeysChallengeMachineKeyFunction
    : public ExtensionFunction {
 public:
  EnterprisePlatformKeysChallengeMachineKeyFunction();

 private:
  ~EnterprisePlatformKeysChallengeMachineKeyFunction() override;
  ResponseAction Run() override;

  // Called when the challenge operation is complete.
  void OnChallengedKey(const ash::attestation::TpmChallengeKeyResult& result);

  EPKPChallengeKey impl_;

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.challengeMachineKey",
                             ENTERPRISE_PLATFORMKEYS_CHALLENGEMACHINEKEY)
};

class EnterprisePlatformKeysChallengeUserKeyFunction
    : public ExtensionFunction {
 public:
  EnterprisePlatformKeysChallengeUserKeyFunction();

 private:
  ~EnterprisePlatformKeysChallengeUserKeyFunction() override;
  ResponseAction Run() override;

  // Called when the challenge operation is complete.
  void OnChallengedKey(const ash::attestation::TpmChallengeKeyResult& result);

  EPKPChallengeKey impl_;

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.challengeUserKey",
                             ENTERPRISE_PLATFORMKEYS_CHALLENGEUSERKEY)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_ASH_H_
