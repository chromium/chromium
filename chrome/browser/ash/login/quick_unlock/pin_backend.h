// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "components/prefs/pref_service.h"

class AccountId;
class Profile;
class ScopedKeepAlive;

namespace ash {
namespace quick_unlock {

class PinStorageCryptohome;
enum class Purpose;

// Provides high-level access to the user's PIN. The underlying storage can be
// either cryptohome or prefs.
class PinBackend : public ash::auth::PinBackendDelegate {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;
  using AvailabilityCallback =
      base::OnceCallback<void(bool, std::optional<base::Time>)>;

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

  PinBackend(const PinBackend&) = delete;
  PinBackend& operator=(const PinBackend&) = delete;

  ~PinBackend() override;

  // Check to see if the PinBackend supports login. This is true when the
  // cryptohome backend is available.
  void HasLoginSupport(BoolCallback result);

  // Try to migrate a prefs-based PIN to cryptohome.
  void MigrateToCryptohome(Profile*, std::unique_ptr<UserContext>);

  // Check if the given account_id has a PIN registered.
  void IsSet(const AccountId& account_id, BoolCallback result);

  // Set the PIN for the given user.
  void Set(const AccountId& account_id,
           const std::string& auth_token,
           const std::string& pin,
           BoolCallback did_set) override;

  // Set the state of PIN auto submit for the given user. Called when enabling
  // auto submit through the confirmation dialog in Settings.
  void SetPinAutoSubmitEnabled(const AccountId& account_id,
                               const std::string& pin,
                               const bool enabled,
                               BoolCallback did_set);

  // Remove the given user's PIN.
  void Remove(const AccountId& account_id,
              const std::string& auth_token,
              BoolCallback did_remove) override;

  // Is PIN authentication available for the given account? Even if PIN is set,
  // it may not be available for authentication due to some additional
  // restrictions.
  void CanAuthenticate(const AccountId& account_id,
                       Purpose purpose,
                       AvailabilityCallback result_callback);

  // Try to check a pin `key` value for the given user. The `key` must be plain
  // text and not contain a salt. The `user_context` must not have an
  // associated auth session, and must have `IsUsingPin` set to true. The
  // UserContext passed to the `result` callback after the authentication
  // attempt is the same as the one that was passed to `TryAuthenticate`. In
  // particular, it does not have an associated auth session.
  void TryAuthenticate(std::unique_ptr<UserContext> user_context,
                       const Key& key,
                       Purpose purpose,
                       AuthOperationCallback result);

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
  void OnPinMigrationAttemptComplete(Profile* profile,
                                     std::unique_ptr<UserContext>,
                                     std::optional<AuthenticationError>);

  // Actions to be performed after an authentication attempt with Cryptohome.
  // The only use case right now is for PIN auto submit, where we might want to
  // expose the PIN length upon a successful attempt.
  void OnCryptohomeAuthenticationResponse(
      const Key& key,
      AuthOperationCallback result,
      std::unique_ptr<UserContext> user_context,
      std::optional<AuthenticationError> error);

  // Called after checking the user's PIN when enabling auto submit.
  // If the authentication was `success`ful, the `pin_length` will be
  // exposed in local state.
  void OnPinAutosubmitCheckComplete(size_t pin_length,
                                    BoolCallback result,
                                    std::unique_ptr<UserContext> user_context,
                                    std::optional<AuthenticationError> error);

  // Help method for working with the PIN auto submit preference.
  PrefService* PrefService(const AccountId& account_id);

  // Simple operations to be performed for PIN auto submit during the common
  // operations in PinBackend - Set, Remove, TryAuthenticate

  void SetWithContext(const AccountId& account_id,
                      const std::string& auth_token,
                      const std::string& pin,
                      BoolCallback did_set,
                      std::unique_ptr<UserContext> user_context);
  void RemoveWithContext(const AccountId& account_id,
                         const std::string& auth_token,
                         BoolCallback did_remove,
                         std::unique_ptr<UserContext> user_context);

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

  // Updates the user context stored in the storage indexed by the supplied
  // `auth_token`, and runs the supplied `callback` with true iff the `error`
  // parameter is `nullopt`.
  static void OnAuthOperation(std::string auth_token,
                              BoolCallback callback,
                              std::unique_ptr<UserContext>,
                              std::optional<AuthenticationError>);

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
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_BACKEND_H_
