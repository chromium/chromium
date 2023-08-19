// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dkrahn): Clean up this private API once all clients have been migrated
// to use the public API. crbug.com/588339.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__

#include <memory>
#include <string>

#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class EPKPChallengeKey {
 public:
  static const char kExtensionNotAllowedError[];
  static const char kChallengeBadBase64Error[];
  EPKPChallengeKey();
  EPKPChallengeKey(const EPKPChallengeKey&) = delete;
  EPKPChallengeKey& operator=(const EPKPChallengeKey&) = delete;
  ~EPKPChallengeKey();

  // Asynchronously run the flow to challenge a key in the |caller|
  // context.
  void Run(::attestation::VerifiedAccessFlow type,
           scoped_refptr<ExtensionFunction> caller,
           ash::attestation::TpmChallengeKeyCallback callback,
           const std::string& challenge,
           bool register_key);

 private:
  // Check if the extension is allowed in the user policy.
  bool IsExtensionAllowed(Profile* profile,
                          scoped_refptr<const Extension> extension);

  std::unique_ptr<ash::attestation::TpmChallengeKey> impl_;
};

class EnterprisePlatformKeysPrivateChallengeMachineKeyFunction
    : public ExtensionFunction {
 public:
  EnterprisePlatformKeysPrivateChallengeMachineKeyFunction();

 private:
  ~EnterprisePlatformKeysPrivateChallengeMachineKeyFunction() override;
  ResponseAction Run() override;

  // Called when the challenge operation is complete.
  void OnChallengedKey(const ash::attestation::TpmChallengeKeyResult& result);

  EPKPChallengeKey impl_;

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.platformKeysPrivate.challengeMachineKey",
      ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEMACHINEKEY)
};

class EnterprisePlatformKeysPrivateChallengeUserKeyFunction
    : public ExtensionFunction {
 public:
  EnterprisePlatformKeysPrivateChallengeUserKeyFunction();

 private:
  ~EnterprisePlatformKeysPrivateChallengeUserKeyFunction() override;
  ResponseAction Run() override;

  // Called when the challenge operation is complete.
  void OnChallengedKey(const ash::attestation::TpmChallengeKeyResult& result);

  EPKPChallengeKey impl_;

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysPrivate.challengeUserKey",
                             ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEUSERKEY)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
