// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_LACROS_H_

#include <string>

#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class LacrosNotImplementedExtensionFunction : public ExtensionFunction {
 protected:
  ~LacrosNotImplementedExtensionFunction() override = default;

 private:
  ResponseAction Run() override;
};

class EnterprisePlatformKeysInternalGenerateKeyFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysInternalGenerateKeyFunction() override = default;
  ResponseAction Run() override;

  using ResultPtr = crosapi::mojom::KeystoreBinaryResultPtr;
  void OnGenerateKey(ResultPtr result);
  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.generateKey",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GENERATEKEY)
};

class EnterprisePlatformKeysGetCertificatesFunction : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysGetCertificatesFunction() override = default;
  ResponseAction Run() override;

  using ResultPtr = crosapi::mojom::GetCertificatesResultPtr;
  void OnGetCertificates(ResultPtr result);
  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.getCertificates",
                             ENTERPRISE_PLATFORMKEYS_GETCERTIFICATES)
};

class EnterprisePlatformKeysImportCertificateFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysImportCertificateFunction() override = default;
  ResponseAction Run() override;

  void OnAddCertificate(const std::string& error);
  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.importCertificate",
                             ENTERPRISE_PLATFORMKEYS_IMPORTCERTIFICATE)
};

class EnterprisePlatformKeysRemoveCertificateFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysRemoveCertificateFunction() override = default;
  ResponseAction Run() override;

  void OnRemoveCertificate(const std::string& error);
  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.removeCertificate",
                             ENTERPRISE_PLATFORMKEYS_REMOVECERTIFICATE)
};

class EnterprisePlatformKeysInternalGetTokensFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysInternalGetTokensFunction() override = default;
  ResponseAction Run() override;

  using ResultPtr = crosapi::mojom::GetKeyStoresResultPtr;
  void OnGetKeyStores(ResultPtr result);
  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysInternal.getTokens",
                             ENTERPRISE_PLATFORMKEYSINTERNAL_GETTOKENS)
};

class EnterprisePlatformKeysChallengeMachineKeyFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysChallengeMachineKeyFunction() override = default;
  ResponseAction Run() override;

  using ResultPtr = crosapi::mojom::KeystoreStringResultPtr;
  void OnChallengeAttestationOnlyKeystore(ResultPtr result);
  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.challengeMachineKey",
                             ENTERPRISE_PLATFORMKEYS_CHALLENGEMACHINEKEY)
};

class EnterprisePlatformKeysChallengeUserKeyFunction
    : public ExtensionFunction {
 private:
  ~EnterprisePlatformKeysChallengeUserKeyFunction() override = default;
  ResponseAction Run() override;

  using ResultPtr = crosapi::mojom::KeystoreStringResultPtr;
  void OnChallengeAttestationOnlyKeystore(ResultPtr result);
  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeys.challengeUserKey",
                             ENTERPRISE_PLATFORMKEYS_CHALLENGEUSERKEY)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_LACROS_H_
