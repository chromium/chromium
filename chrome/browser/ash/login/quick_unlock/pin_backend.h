// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_

#include <string>

#include "base/callback.h"
#include "chromeos/login/auth/key.h"
#include "components/prefs/pref_service.h"

class AccountId;
class Profile;
class ScopedKeepAlive;

namespace chromeos {

namespace quick_unlock {

class PinStorageCryptohome;

// Provides high-level access to the user's PIN. The underlying storage can be
// either cryptohome or prefs.
class PinBackend {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;

  // Fetch the PinBackend instance.
  static PinBackend* GetInstance();

  // Computes a new salt.
  static std::string ComputeSalt();

  // Computes the secret for a given `pin` and `salt`.
  static std::string ComputeSecret(const std::string& pin,
                                   const std::string& salt,
                                   Key::KeyType key_type);

  // Use GetInstance().
  PinBackend();
  ~PinBackend();

  // Check to see if the PinBackend supports login. This is true when the
  // cryptohome backend is available.
  void HasLoginSupport(BoolCallback result);

  // Try to migrate a prefs-based PIN to cryptohome.
  void MigrateToCryptohome(Profile* profile, const Key& key);

  // Check if the given account_id has a PIN registered.
  void IsSet(const AccountId& account_id, BoolCallback result);

  // Set the PIN for the given user.
  void Set(const AccountId& account_id,
           const std::string& auth_token,
           const std::string& pin,
           BoolCallback did_set);

  // Set the state of PIN auto submit for the given user. Called when enabling
  // auto submit through the confirmation dialog in Settings.
  void SetPinAutoSubmitEnabled(const AccountId& account_id,
                               const std::string& pin,
                               const bool enabled,
                               BoolCallback did_set);

  // Remove the given user's PIN.
  void Remove(const AccountId& account_id,
              const std::string& auth_token,
              BoolCallback did_remove);

  // Is PIN authentication available for the given account? Even if PIN is set,
  // it may not be available for authentication due to some additional
  // restrictions.
  void CanAuthenticate(const AccountId& account_id, BoolCallback result);

  // Try to authenticate.
  void TryAuthenticate(const AccountId& account_id,
                       const Key& key,
                       BoolCallback result);

  // Returns true if the cryptohome backend should be used. Sometimes the prefs
  // backend should be used even when cryptohome is available, ie, when there is
  // an non-migrated PIN key.
  bool ShouldUseCryptohome(const AccountId& account_id);

  // Resets any cached state for testing purposes.
  static void ResetForTesting();

  // Interface for the lock/login screen to access the user's PIN length.
  // Ensures that the UI is always consistent with the pref values without the
  // need for individual observers.
  int GetExposedPinLength(const AccountId& account_id);

  // TODO(crbug.com/1104164) - Remove this once most users have their
  // preferences backfilled.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Must coincide with the enum
  // PinAutosubmitBackfillEvent on enums.xml
  enum class BackfillEvent {
    // Successfully set the user preference on the server
    kEnabled = 0,
    // Possible errors to keep track of.
    kDisabledDueToPolicy = 1,
    kDisabledDueToPinLength = 2,
    kMaxValue = kDisabledDueToPinLength,
  };

 private:
  // Called when we know if the cryptohome supports PIN.
  void OnIsCryptohomeBackendSupported(bool is_supported);

  // Called when a migration attempt has completed. If `success` is true the PIN
  // should be cleared from prefs.
  void OnPinMigrationAttemptComplete(Profile* profile, bool success);

  // Actions to be performed after an authentication attempt with Cryptohome.
  // The only use case right now is for PIN auto submit, where we might want to
  // expose the PIN length upon a successful attempt.
  void OnCryptohomeAuthenticationResponse(const AccountId& account_id,
                                          const Key& key,
                                          BoolCallback result,
                                          bool success);

  // Called after checking the user's PIN when enabling auto submit.
  // If the authentication was `success`ful, the `pin_length` will be
  // exposed in local state.
  void OnPinAutosubmitCheckComplete(const AccountId& account_id,
                                    size_t pin_length,
                                    BoolCallback result,
                                    bool success);

  // Help method for working with the PIN auto submit preference.
  PrefService* PrefService(const AccountId& account_id);

  // Simple operations to be performed for PIN auto submit during the common
  // operations in PinBackend - Set, Remove, TryAuthenticate

  // When setting/updating a PIN. After every 'Set' operation the
  // exposed length can only be either the true PIN length, or zero.
  void UpdatePinAutosubmitOnSet(const AccountId& account_id, size_t pin_length);

  // Clears the exposed PIN length and resets the user setting.
  void UpdatePinAutosubmitOnRemove(const AccountId& account_id);

  // A successful authentication attempt will expose the pin length. This is
  // necessary when the preference is being set by policy. When the pref is
  // being controlled by the user -- through Settings --, the length is exposed
  // through a confirmation dialog immediately.
  void UpdatePinAutosubmitOnSuccessfulTryAuth(const AccountId& account_id,
                                              size_t pin_length);

  // PIN auto submit backfill operation for users with existing PINs.
  // TODO(crbug.com/1104164) - Remove this once most users have their
  // preferences backfilled.
  // Backfill PIN auto submit preferences for users who already have a pin, but
  // had it set up before the pin auto submit feature was released.
  void PinAutosubmitBackfill(const AccountId& account_id, size_t pin_length);

  // True if still trying to determine which backend should be used.
  bool resolving_backend_ = true;
  // Determining if the device supports cryptohome-based keys requires an async
  // dbus call to cryptohome. If we receive a request before we know which
  // backend to use, the request will be pushed to this list and invoked once
  // the backend configuration is determined.
  std::vector<base::OnceClosure> on_cryptohome_support_received_;

  // Non-null if we should use the cryptohome backend. If null, the prefs
  // backend should be used.
  std::unique_ptr<PinStorageCryptohome> cryptohome_backend_;

  // Blocks chrome from restarting while migrating from prefs to cryptohome PIN.
  std::unique_ptr<ScopedKeepAlive> scoped_keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(PinBackend);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_
