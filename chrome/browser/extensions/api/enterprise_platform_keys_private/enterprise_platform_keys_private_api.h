// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dkrahn): Clean up this private API once all clients have been migrated
// to use the public API. crbug.com/588339.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__

#include <memory>
#include <string>

#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class EPKPChallengeKey {
 public:
  static const char kExtensionNotWhitelistedError[];
  static const char kChallengeBadBase64Error[];
  EPKPChallengeKey();
  EPKPChallengeKey(const EPKPChallengeKey&) = delete;
  EPKPChallengeKey& operator=(const EPKPChallengeKey&) = delete;
  ~EPKPChallengeKey();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Asynchronously run the flow to challenge a key in the |caller|
  // context.
  void Run(chromeos::attestation::AttestationKeyType type,
           scoped_refptr<ExtensionFunction> caller,
           chromeos::attestation::TpmChallengeKeyCallback callback,
           const std::string& challenge,
           bool register_key);

 private:
  // Check if the extension is whitelisted in the user policy.
  bool IsExtensionWhitelisted(Profile* profile,
                              scoped_refptr<const Extension> extension);

  std::unique_ptr<chromeos::attestation::TpmChallengeKey> impl_;
};

class EnterprisePlatformKeysPrivateChallengeMachineKeyFunction
    : public ExtensionFunction {
 public:
  EnterprisePlatformKeysPrivateChallengeMachineKeyFunction();

 private:
  ~EnterprisePlatformKeysPrivateChallengeMachineKeyFunction() override;
  ResponseAction Run() override;

  // Called when the challenge operation is complete.
  void OnChallengedKey(
      const chromeos::attestation::TpmChallengeKeyResult& result);

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
  void OnChallengedKey(
      const chromeos::attestation::TpmChallengeKeyResult& result);

  EPKPChallengeKey impl_;

  DECLARE_EXTENSION_FUNCTION("enterprise.platformKeysPrivate.challengeUserKey",
                             ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEUSERKEY)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
