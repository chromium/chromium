// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CHALLENGE_RESPONSE_AUTH_KEYS_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CHALLENGE_RESPONSE_AUTH_KEYS_LOADER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "net/ssl/client_cert_identity.h"

class AccountId;

namespace chromeos {

// This class allows to prepare parameters for the challenge-response
// authentication.
//
// This authentication is based on making challenges against cryptographic
// key(s). The challenge generation and processing the authentication secrets
// is performed by the cryptohomed daemon and the underlying layers. The browser
// is responsible for providing the cryptohomed with the public key information
// (which is the responsibility of this class) and for forwarding the challenge
// requests to the component that talks to the cryptographic token (which is the
// responsibility of CryptohomeKeyDelegateServiceProvider).
class ChallengeResponseAuthKeysLoader final {
 public:
  using LoadAvailableKeysCallback = base::OnceCallback<void(
      std::vector<ChallengeResponseKey> challenge_response_keys)>;

  // Returns whether the given user, whose profile must already exist on the
  // device, supports authentication via the challenge-response protocol.
  static bool CanAuthenticateUser(const AccountId& account_id);

  ChallengeResponseAuthKeysLoader();
  ~ChallengeResponseAuthKeysLoader();

  // Prepares the ChallengeResponseKey values containing the currently available
  // cryptographic keys that can be used to authenticate the given user.
  //
  // The callback is run with an empty |challenge_response_keys| in the cases
  // when the user's profile doesn't support challenge-response authentication
  // or when there's no currently available suitable cryptographic key.
  void LoadAvailableKeys(const AccountId& account_id,
                         LoadAvailableKeysCallback callback);

 private:
  // Asynchronous job which is scheduled by LoadAvailableKeys after the list of
  // currently available cryptographic keys is refreshed from certificate
  // providers.
  void ContinueLoadAvailableKeysWithCerts(
      const AccountId& account_id,
      const std::vector<std::string>& suitable_public_key_spki_items,
      LoadAvailableKeysCallback callback,
      net::ClientCertIdentityList cert_identities);

  base::WeakPtrFactory<ChallengeResponseAuthKeysLoader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChallengeResponseAuthKeysLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CHALLENGE_RESPONSE_AUTH_KEYS_LOADER_H_
