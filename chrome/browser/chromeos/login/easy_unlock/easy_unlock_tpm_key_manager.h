// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "crypto/scoped_nss_types.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// Manages per user RSA keys stored in system TPM slot used in easy signin
// protocol. The keys are used to sign a nonce exchanged during signin.
class EasyUnlockTpmKeyManager : public KeyedService {
 public:
  // Registers local state prefs used to store public RSA keys per user.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Clears local state for user. Should be called when a user is removed.
  static void ResetLocalStateForUser(const AccountId& account_id);

  // |account_id|: Id for the user associated with the service. Empty for
  //     sign-in service.
  // |username_hash|: Username hash for the user associated with the service.
  //     Empty for sign-in service.
  // |local_state|: The local state prefs.
  EasyUnlockTpmKeyManager(const AccountId& account_id,
                          const std::string& username_hash,
                          PrefService* local_state);
  ~EasyUnlockTpmKeyManager() override;

  // Checks if the RSA public key is set in the local state. If not, creates
  // one. If the key presence can be confirmed, immediately returns true and
  // |callback| never gets called, otherwise returns false (callback is called
  // when the key presence is confirmed).
  // Must not be called for signin profile.
  // |check_private_key|: If public RSA key is set in the local state, whether
  //     the method should confirm that the private key is present in the system
  //     slot. If the private key cannot be found, a new key pair will be
  //     created for the user.
  //     Note: Checking TPM for the private key is more expensive than only
  //     checking local state, so setting this to |false| should be preferable.
  //     Generally, if public key is set in local state, the private key should
  //     be present in the system TPM slot. This is used to make sure that easy
  //     signin does not remain permanently broken if something goes wrong.
  // |callback|: If the method cannot return immediately, called when the key
  //     pair presence is confirmed (or a key pair for the user is created).
  bool PrepareTpmKey(bool check_private_key, const base::Closure& callback);

  // If called, posts a delayed task that cancels |PrepareTpmKey| and all other
  // started timeouts in case getting system slot takes more than |timeout_ms|.
  // In the case getting system slot times out, |PrepareTpmKey| callback will
  // be called with an empty public key.
  // Must be called after |PrepareTpmKey| to have the intended effect.
  bool StartGetSystemSlotTimeoutMs(size_t timeout_ms);

  // Gets the public RSA key for user. The key is retrieved from local state.
  std::string GetPublicTpmKey(const AccountId& account_id);

  // Signs |data| using private RSA key associated with |user_id| stored in TPM
  // system slot.
  void SignUsingTpmKey(
      const AccountId& account_id,
      const std::string& data,
      const base::Callback<void(const std::string& data)> callback);

  bool StartedCreatingTpmKeys() const;

 private:
  enum CreateTpmKeyState {
    CREATE_TPM_KEY_NOT_STARTED,
    CREATE_TPM_KEY_WAITING_FOR_USER_SLOT,
    CREATE_TPM_KEY_WAITING_FOR_SYSTEM_SLOT,
    CREATE_TPM_KEY_GOT_SYSTEM_SLOT,
    CREATE_TPM_KEY_DONE
  };

  // Utility method for setting public key values in local state.
  // Note that the keys are saved base64 encoded.
  void SetKeyInLocalState(const AccountId& account_id,
                          const std::string& value);

  // Called when TPM system slot is initialized and ready to be used.
  // It creates RSA key pair for the user in the system slot.
  // When the key pair is created, |OnTpmKeyCreated| will be called with the
  // created public key.
  // The key will not be created if |public_key| is non-empty and the associated
  // private key can be found in the slot. Instead |OnTpmKeyCreated| will be
  // called with |public_key|.
  void CreateKeyInSystemSlot(const std::string& public_key,
                             crypto::ScopedPK11Slot system_slot);

  // Called when user TPM token initialization is done. After this happens,
  // |this| may proceed with creating a user-specific TPM key for easy sign-in.
  // Note that this is done solely to ensure user TPM initialization, which is
  // done on IO thread, is not blocked by creating TPM keys in system slot.
  void OnUserTPMInitialized(const std::string& public_key);

  // Called when TPM system slot is initialized and ready to be used.
  // It schedules data signing operation on a worker thread. The data is signed
  // by a private key stored in |system_slot| and identified by |public_key|
  // (a private key that is part of the same RSA key pair as |public_key|).
  // Once data is signed |callback| is called with the signed data.
  void SignDataWithSystemSlot(
      const std::string& public_key,
      const std::string& data,
      const base::Callback<void(const std::string& data)> callback,
      crypto::ScopedPK11Slot system_slot);

  // Called when a RSA key pair is created for a user in TPM system slot.
  // It saves the pulic key in the local state and runs queued up
  // |PrepareTpmKey| callbacks.
  void OnTpmKeyCreated(const std::string& public_key);

  // Called when data signing requested in |SignUsingTpmKey| is done.
  // It runs |callback| with the created |signature|. On error the callback will
  // be run with an empty string.
  void OnDataSigned(const base::Callback<void(const std::string&)>& callback,
                    const std::string& signature);

  const AccountId account_id_;
  std::string username_hash_;

  PrefService* local_state_;

  // The current TPM key creation state. If key creation is in progress,
  // callbacks for further |PrepareTpmKey| will be queued up and run when the
  // key is created. All queued callbacks will be run with the same key value.
  CreateTpmKeyState create_tpm_key_state_;

  // Queued up |PrepareTpmKey| callbacks.
  std::vector<base::Closure> prepare_tpm_key_callbacks_;

  base::WeakPtrFactory<EasyUnlockTpmKeyManager> get_tpm_slot_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<EasyUnlockTpmKeyManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockTpmKeyManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_TPM_KEY_MANAGER_H_
