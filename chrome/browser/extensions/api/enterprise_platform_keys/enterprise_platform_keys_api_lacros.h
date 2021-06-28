// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_LACROS_H_

#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class LacrosNotImplementedExtensionFunction : public ExtensionFunction {
 protected:
  ~LacrosNotImplementedExtensionFunction() override = default;

 private:
  ResponseAction Run() override;
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
