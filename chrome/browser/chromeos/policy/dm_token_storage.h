// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DM_TOKEN_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DM_TOKEN_STORAGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

// Helper class to store/retrieve DM token to/from the local state. This is
// needed for Active Directory management because AD devices lacks DM token in
// the policies. DM token will be used in the future for ARC integration.
//
// Note that requests must be made from the UI thread because SystemSaltGetter
// calls CryptohomeClient which must be called from the UI thread.
class DMTokenStorage {
 public:
  using StoreCallback = base::OnceCallback<void(bool success)>;
  using RetrieveCallback =
      base::OnceCallback<void(const std::string& dm_token)>;

  explicit DMTokenStorage(PrefService* local_state);
  ~DMTokenStorage();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Persists |dm_token| on the device. Overwrites any previous value. Signals
  // completion via |callback|, passing true if the operation succeeded. Fails
  // if another operation is running (store or retrieve).
  void StoreDMToken(const std::string& dm_token, StoreCallback callback);

  // Loads DM token from the local state and decrypts it. Fires callback on
  // completion. Empty |dm_token| means error. Calls |callback| with empty token
  // if store operation is running.
  void RetrieveDMToken(RetrieveCallback callback);

 private:
  enum class SaltState {
    // Pending system salt.
    LOADING,
    // Failed to load system salt.
    ERROR,
    // System salt is loaded.
    LOADED,
  };

  // Callback for SystemSaltRetrieveter.
  void OnSystemSaltRecevied(const std::string& system_salt);

  // Encrypts DM token using system salt and stores it into the local state.
  void EncryptAndStoreToken();

  // Callback waiting for DM token to be encrypted.
  void OnTokenEncrypted(const std::string& encrypted_dm_token);

  // Loads encrypted DM token from the local state and decrypts it using system
  // salt.
  void LoadAndDecryptToken();

  // Fires StoreCallback (if exists) with the status.
  void FlushStoreTokenCallback(bool status);

  // Fires RetrieveCallbacks (if exists) with |dm_token|.
  void FlushRetrieveTokenCallback(const std::string& dm_token);

  PrefService* local_state_;
  SaltState state_ = SaltState::LOADING;
  std::string system_salt_;
  // Stored |dm_token| while waiting for system salt.
  std::string dm_token_;
  StoreCallback store_callback_;
  std::vector<RetrieveCallback> retrieve_callbacks_;
  base::WeakPtrFactory<DMTokenStorage> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DM_TOKEN_STORAGE_H_
