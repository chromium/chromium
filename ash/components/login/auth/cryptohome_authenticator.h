// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_CRYPTOHOME_AUTHENTICATOR_H_
#define ASH_COMPONENTS_LOGIN_AUTH_CRYPTOHOME_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "ash/components/login/auth/auth_attempt_state.h"
#include "ash/components/login/auth/authenticator.h"
#include "ash/components/login/auth/safe_mode_delegate.h"
#include "ash/components/login/auth/test_attempt_state.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AuthFailure;
class PrefService;

namespace ash {

class AuthStatusConsumer;
class CryptohomeAuthenticatorTest;

// Authenticates a Chromium OS user against cryptohome.
// Relies on the fact that online authentications has been already performed
// (i.e. using_oauth_ is true).
//
// At a high, level, here's what happens:
// AuthenticateToLogin() calls a Cryptohome's method to perform offline login.
// Results are stored in a AuthAttemptState owned by CryptohomeAuthenticator
// and then call Resolve().  Resolve() will attempt to
// determine which AuthState we're in, based on the info at hand.
// It then triggers further action based on the calculated AuthState; this
// further action might include calling back the passed-in AuthStatusConsumer
// to signal that login succeeded or failed, waiting for more outstanding
// operations to complete, or triggering some more Cryptohome method calls.
//
// Typical flows
// -------------
// Add new user: CONTINUE > CONTINUE > CREATE_NEW > CONTINUE > ONLINE_LOGIN
// Login as existing user: CONTINUE > OFFLINE_LOGIN
// Login as existing user (failure): CONTINUE > FAILED_MOUNT
// Change password detected:
//   GAIA online ok: CONTINUE > CONTINUE > NEED_OLD_PW
//     Recreate: CREATE_NEW > CONTINUE > ONLINE_LOGIN
//     Old password failure: NEED_OLD_PW
//     Old password ok: RECOVER_MOUNT > CONTINUE > ONLINE_LOGIN
//
class COMPONENT_EXPORT(ASH_LOGIN_AUTH) CryptohomeAuthenticator
    : public Authenticator {
 public:
  enum AuthState {
    CONTINUE = 0,            // State indeterminate; try again with more info.
    NO_MOUNT = 1,            // Cryptohome doesn't exist yet.
    FAILED_MOUNT = 2,        // Failed to mount existing cryptohome.
    FAILED_REMOVE = 3,       // Failed to remove existing cryptohome.
    FAILED_TMPFS = 4,        // Failed to mount tmpfs for guest user.
    FAILED_TPM = 5,          // Failed to mount/create cryptohome, TPM error.
    CREATE_NEW = 6,          // Need to create cryptohome for a new user.
    RECOVER_MOUNT = 7,       // After RecoverEncryptedData, mount cryptohome.
    POSSIBLE_PW_CHANGE = 8,  // Offline login failed, user may have changed pw.
    NEED_NEW_PW = 9,         // Obsolete (ClientLogin): user changed pw,
                             // we have the old one.
    NEED_OLD_PW = 10,        // User changed pw, and we have the new one
                             // (GAIA auth is OK).
    HAVE_NEW_PW = 11,        // Obsolete (ClientLogin): We have verified new pw,
                             // time to migrate key.
    OFFLINE_LOGIN = 12,      // Login succeeded offline.
    ONLINE_LOGIN = 13,       // Offline and online login succeeded.
    UNLOCK = 14,             // Obsolete: Screen unlock succeeded.
    ONLINE_FAILED = 15,      // Obsolete (ClientLogin): Online login disallowed,
                             // but offline succeeded.
    GUEST_LOGIN = 16,        // Logged in guest mode.
    PUBLIC_ACCOUNT_LOGIN = 17,  // Logged into a public account.
    // SUPERVISED_USER_LOGIN_DEPRECATED = 18,
    LOGIN_FAILED = 19,                // Obsolete: Login denied.
    OWNER_REQUIRED = 20,              // Login is restricted to the owner only.
    FAILED_USERNAME_HASH = 21,        // Failed GetSanitizedUsername request.
    KIOSK_ACCOUNT_LOGIN = 22,         // Logged into a kiosk account.
    REMOVED_DATA_AFTER_FAILURE = 23,  // Successfully removed the user's
                                      // cryptohome after a login failure.
    FAILED_OLD_ENCRYPTION = 24,       // Login failed, cryptohome is encrypted
                                      // in old format.
    FAILED_PREVIOUS_MIGRATION_INCOMPLETE = 25,  // Login failed, cryptohome is
                                                // partially encrypted in old
                                                // format.
    OFFLINE_NO_MOUNT = 26,  // Offline login failed due to missing cryptohome.
    TPM_UPDATE_REQUIRED = 27,          // TPM firmware update is required.
    OFFLINE_MOUNT_UNRECOVERABLE = 28,  // Offline login failed due to corrupted
                                       // cryptohome.
  };

  CryptohomeAuthenticator(scoped_refptr<base::SequencedTaskRunner> task_runner,
                          PrefService* local_state,
                          std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
                          AuthStatusConsumer* consumer);

  CryptohomeAuthenticator(const CryptohomeAuthenticator&) = delete;
  CryptohomeAuthenticator& operator=(const CryptohomeAuthenticator&) = delete;

  // Authenticator overrides.
  void CompleteLogin(std::unique_ptr<UserContext> user_context) override;

  // Given |user_context|, this method attempts to authenticate to your
  // Chrome OS device. As soon as we have successfully mounted the encrypted
  // home directory for the user, we will call consumer_->OnAuthSuccess()
  // with the username.
  // Upon failure to login consumer_->OnAuthFailure() is called
  // with an error message.
  void AuthenticateToLogin(std::unique_ptr<UserContext> user_context) override;

  // Initiates incognito ("browse without signing in") login.
  // Mounts tmpfs and notifies consumer on the success/failure.
  void LoginOffTheRecord() override;

  // Initiates login into a public session.
  // Mounts an ephemeral cryptohome and notifies consumer on the
  // success/failure.
  void LoginAsPublicSession(const UserContext& user_context) override;

  // Initiates login into kiosk mode account identified by |app_account_id|.
  // The |app_account_id| is a generated account id for the account.
  // So called Public mount is used to mount cryptohome.
  void LoginAsKioskAccount(const AccountId& app_account_id) override;

  // Initiates login into the ARC kiosk mode account identified by
  // |app_account_id|.
  // Mounts a public cryptohome, which will be ephemeral if the
  // |DeviceEphemeralUsersEnabled| policy is enabled and non-ephemeral
  // otherwise.
  void LoginAsArcKioskAccount(const AccountId& app_account_id) override;

  // Initiates login into the web kiosk mode account identified by
  // |app_account_id|.
  // Mounts a public cryptohome, which will be ephemeral if the
  // |DeviceEphemeralUsersEnabled| policy is enabled and non-ephemeral
  // otherwise.
  void LoginAsWebKioskAccount(const AccountId& app_account_id) override;

  // These methods must be called on the UI thread, as they make DBus calls
  // and also call back to the login UI.
  void OnAuthSuccess() override;
  void OnAuthFailure(const AuthFailure& error) override;
  void RecoverEncryptedData(std::unique_ptr<UserContext> user_context,
                            const std::string& old_password) override;
  void ResyncEncryptedData(std::unique_ptr<UserContext> user_context) override;

  // Called after UnmountEx finishes.
  void OnUnmountEx(absl::optional<user_data_auth::UnmountReply> reply);

  // Attempts to make a decision and call back |consumer_| based on
  // the state we have gathered at the time of call.  If a decision
  // can't be made, defers until the next time this is called.
  // When a decision is made, will call back to |consumer_| on the UI thread.
  //
  // Must be called on the UI thread.
  void Resolve();

  void OnOffTheRecordAuthSuccess();
  void OnPasswordChangeDetected();
  void OnOldEncryptionDetected(bool has_incomplete_migration);

  // Migrate cryptohome key for |user_context| using |old_password|.
  void MigrateKey(const UserContext& user_context,
                  const std::string& old_password);

 protected:
  ~CryptohomeAuthenticator() override;

 private:
  friend class CryptohomeAuthenticatorTest;
  FRIEND_TEST_ALL_PREFIXES(CryptohomeAuthenticatorTest,
                           ResolveOwnerNeededDirectFailedMount);
  FRIEND_TEST_ALL_PREFIXES(CryptohomeAuthenticatorTest,
                           ResolveOwnerNeededMount);
  FRIEND_TEST_ALL_PREFIXES(CryptohomeAuthenticatorTest,
                           ResolveOwnerNeededFailedMount);

  // Removes the cryptohome of the user.
  void RemoveEncryptedData();

  // Returns the AuthState we're in, given the status info we have at
  // the time of call.
  // Must be called on the IO thread.
  AuthState ResolveState();

  // Helper for ResolveState().
  // Given that some cryptohome operation has failed, determine which of the
  // possible failure states we're in.
  // Must be called on the IO thread.
  AuthState ResolveCryptohomeFailureState();

  // Helper for ResolveState().
  // Given that some cryptohome operation has succeeded, determine which of
  // the possible states we're in.
  // Must be called on the IO thread.
  AuthState ResolveCryptohomeSuccessState();

  // Helper for ResolveState().
  // Given that some online auth operation has succeeded, determine which of
  // the possible success states we're in.
  // Must be called on the IO thread.
  AuthState ResolveOnlineSuccessState(AuthState offline_state);

  // Used for testing.
  void set_attempt_state(TestAttemptState* new_state) {  // takes ownership.
    current_state_.reset(new_state);
  }

  // Used for testing to set the expected state of an owner check.
  void SetOwnerState(bool owner_check_finished, bool check_result);

  // checks if the current mounted home contains the owner case and either
  // continues or fails the log-in. Used for policy lost mitigation "safe-mode".
  // Returns true if the owner check has been successful or if it is not needed.
  bool VerifyOwner();

  // Handles completion of the ownership check and continues login.
  void OnOwnershipChecked(bool is_owner);

  // Handles completion of cryptohome unmount.
  void OnUnmount(absl::optional<bool> success);

  // Signal login completion status for cases when a new user is added via
  // an external authentication provider (i.e. GAIA extension).
  void ResolveLoginCompletionStatus();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  const base::raw_ptr<PrefService> local_state_;

  std::unique_ptr<SafeModeDelegate> safe_mode_delegate_;

  std::unique_ptr<AuthAttemptState> current_state_;
  bool migrate_attempted_ = false;
  bool remove_attempted_ = false;
  bool resync_attempted_ = false;
  bool ephemeral_mount_attempted_ = false;

  // When the user has changed their password, but gives us the old one, we will
  // be able to mount their cryptohome, but online authentication will fail.
  // This allows us to present the same behavior to the caller, regardless
  // of the order in which we receive these results.
  bool already_reported_success_ = false;
  base::Lock success_lock_;  // A lock around |already_reported_success_|.

  // Flags signaling whether the owner verification has been done and the result
  // of it.
  bool owner_is_verified_ = false;
  bool user_can_login_ = false;

  // Flag indicating to delete the user's cryptohome the login fails.
  bool remove_user_data_on_failure_ = false;

  // When |remove_user_data_on_failure_| is set, we delay calling
  // consumer_->OnAuthFailure() until we removed the user cryptohome.
  AuthFailure delayed_login_failure_;
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_CRYPTOHOME_AUTHENTICATOR_H_
