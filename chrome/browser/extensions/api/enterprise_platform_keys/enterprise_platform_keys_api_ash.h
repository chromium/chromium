// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_API_ASH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "extensions/browser/extension_function.h"

namespace net {
class X509Certificate;
using CertificateList = std::vector<scoped_refptr<X509Certificate>>;
}  // namespace net

namespace extensions {

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
