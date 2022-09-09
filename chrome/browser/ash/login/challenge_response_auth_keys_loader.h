// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_CHALLENGE_RESPONSE_AUTH_KEYS_LOADER_H_
#define CHROME_BROWSER_ASH_LOGIN_CHALLENGE_RESPONSE_AUTH_KEYS_LOADER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "net/ssl/client_cert_identity.h"

class AccountId;

namespace ash {

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
class ChallengeResponseAuthKeysLoader final : public ProfileObserver {
 public:
  using LoadAvailableKeysCallback = base::OnceCallback<void(
      std::vector<ChallengeResponseKey> challenge_response_keys)>;

  // Returns whether the given user, whose profile must already exist on the
  // device, supports authentication via the challenge-response protocol.
  static bool CanAuthenticateUser(const AccountId& account_id);

  ChallengeResponseAuthKeysLoader();
  ChallengeResponseAuthKeysLoader(const ChallengeResponseAuthKeysLoader&) =
      delete;
  ChallengeResponseAuthKeysLoader& operator=(
      const ChallengeResponseAuthKeysLoader&) = delete;
  ~ChallengeResponseAuthKeysLoader() override;

  // Prepares the ChallengeResponseKey values containing the currently available
  // cryptographic keys that can be used to authenticate the given user. If
  // there should be force-installed extensions that provide a certificate for
  // the given user, waits until these are installed and loaded (default: up to
  // 5 seconds, configured by `maximum_extension_load_waiting_time_`).
  //
  // The callback is run with an empty `challenge_response_keys` in the cases
  // when the user's profile doesn't support challenge-response authentication
  // or when there is no suitable cryptographic key available.
  void LoadAvailableKeys(const AccountId& account_id,
                         LoadAvailableKeysCallback callback);

  void SetMaxWaitTimeForTesting(base::TimeDelta time) {
    maximum_extension_load_waiting_time_ = time;
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  // Asynchronous job which is scheduled by LoadAvailableKeys after all
  // necessary extensions are loaded.
  void ContinueLoadAvailableKeysExtensionsLoaded(
      const AccountId& account_id,
      const std::vector<std::string>& suitable_public_key_spki_items,
      LoadAvailableKeysCallback callback);

  // Asynchronous job which is scheduled by
  // ContinueLoadAvailableKeysExtensionsLoaded after the list of currently
  // available cryptographic keys is refreshed from certificate providers.
  void ContinueLoadAvailableKeysWithCerts(
      const AccountId& account_id,
      const std::vector<std::string>& suitable_public_key_spki_items,
      LoadAvailableKeysCallback callback,
      net::ClientCertIdentityList cert_identities);

  base::TimeDelta maximum_extension_load_waiting_time_;

  // Whether the sign-in profile is destroyed.
  bool profile_is_destroyed_ = false;

  base::ScopedObservation<Profile, ProfileObserver> profile_subscription_{this};

  base::WeakPtrFactory<ChallengeResponseAuthKeysLoader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_CHALLENGE_RESPONSE_AUTH_KEYS_LOADER_H_
