// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_
#define ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_

#include "ash/components/login/auth/authenticator.h"
#include "ash/components/login/auth/safe_mode_delegate.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AuthFailure;

namespace ash {

class AuthStatusConsumer;

// Authenticator that authenticates user against ChromeOS cryptohome using
// AuthSession API.
// Parallel authentication attempts are not possible, this is guarded by
// resetting all callbacks bound to weak pointers.
// Generic flow for all authentication attempts:
// * Initialize AuthSession (and learn if user exists)
//   * For existing users:
//     * Transform keys if necessary
//     * Authenticate AuthSession
//     * Mount directory
//     * (Safe mode) Check ownership
//   * For new users:
//     * Transform keys if necessary
//     * Add user credentials
//     * Authenticate session with same credentials
//     * Mount home directory (with empty create request)

// There are several points where flows are customized:
//   * Additional configuration of StartAuthSessionRequest
//   * Additional configuration of MountRequest
//   * Customized error handling
//     (e.g. different "default" error for different user types)
//   * Different ways to hash plain text key
//   * Different ways to create crytohome key from key in UserContext

class COMPONENT_EXPORT(ASH_LOGIN_AUTH) AuthSessionAuthenticator
    : public Authenticator {
 public:
  AuthSessionAuthenticator(AuthStatusConsumer* consumer,
                           std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
                           bool is_ephemeral_mount_enforced);

  // Authenticator overrides.
  void CompleteLogin(std::unique_ptr<UserContext> user_context) override;

  void AuthenticateToLogin(std::unique_ptr<UserContext> user_context) override;
  void LoginOffTheRecord() override;
  void LoginAsPublicSession(const UserContext& user_context) override;
  void LoginAsKioskAccount(const AccountId& app_account_id) override;
  void LoginAsArcKioskAccount(const AccountId& app_account_id) override;
  void LoginAsWebKioskAccount(const AccountId& app_account_id) override;
  void OnAuthSuccess() override;
  void OnAuthFailure(const AuthFailure& error) override;
  void RecoverEncryptedData(std::unique_ptr<UserContext> user_context,
                            const std::string& old_password) override;
  void ResyncEncryptedData(std::unique_ptr<UserContext> user_context) override;

 protected:
  ~AuthSessionAuthenticator() override;

 private:
  // -------------------------------------------------------------------
  // ---- Various callback types used to define authentication flow ----
  // -------------------------------------------------------------------

  // Generic callback type, as UserContext is passed along the login flow.
  using ContextCallback =
      base::OnceCallback<void(std::unique_ptr<UserContext>)>;

  // Error handling callback, translates CryptohomeErrorCode to AuthFailure.
  using ErrorHandlingCallback =
      base::OnceCallback<void(std::unique_ptr<UserContext>,
                              user_data_auth::CryptohomeErrorCode)>;
  // Continuation used for newly created users, added for clarity.
  using NewUserAuthSessionCallback = ContextCallback;
  // Continuation used for returning users, added for clarity.
  using ExistingUserAuthSessionCallback = ContextCallback;

  // Function that would hash a key inside `context` so that it can be safely
  // passed to cryptohome over DBus.
  // Hashing uses system salt and optionally key metadata
  // (`UserKeys` in `context`).
  // As this operation might require getting of system salt, it is an
  // asynchronous operation.
  using KeyHashingCallback =
      base::OnceCallback<void(std::unique_ptr<UserContext> context,
                              ContextCallback continuation)>;

  // -------------------------------------------------------------------
  // ---- Various synchronous callbacks that are used to configure  ----
  // ---- requests to cryptohome.                                   ----
  //--------------------------------------------------------------------

  using ConfigureAuthSessionCallback =
      base::OnceCallback<user_data_auth::StartAuthSessionRequest(
          const UserContext& context,
          user_data_auth::StartAuthSessionRequest request)>;

  using ConfigureMountCallback =
      base::OnceCallback<user_data_auth::MountRequest(
          const UserContext& context,
          user_data_auth::MountRequest request)>;

  // Transforms ash::Key in UserContext to cryptohome::KeyDefinition
  // used in cryptohome requests.
  using TransformCryotohomeKeyCallback =
      base::OnceCallback<void(const UserContext& context,
                              cryptohome::Key* out_key)>;

  // -------------------------------------------------------------------
  // ---- Cryptohome operation wrappers that can be used to build   ----
  // ---- authentication flows                                      ----
  //--------------------------------------------------------------------

  // General order of parameters:
  //  - Logging-only ids
  //  - Error handler
  //  - Transformers
  //  - Continuations
  //  - UserContext (as it is often left unbounded)
  //  - ...RequestReply (as it is left unbounded)

  // Starts new AuthSession and passes parameters to
  // `OnAuthSessionCreatedGeneric`.
  void CreateAuthSessionGeneric(
      const std::string& user_type,
      ErrorHandlingCallback error_handler,
      ConfigureAuthSessionCallback configurator,
      KeyHashingCallback key_hasher,
      NewUserAuthSessionCallback new_user_flow,
      ExistingUserAuthSessionCallback existing_user_flow,
      std::unique_ptr<UserContext> context);

  // AuthSession creation callback:
  //  * Hashes key in `context` by invoking `key_hasher`
  //     and using metadata in reply
  //  * Continues with either `new_user_callback` or `existing_user_callback`,
  //    depending on the user existence.
  // `user_type` is used only for logging purposes.
  void OnAuthSessionCreatedGeneric(
      const std::string& user_type,
      ErrorHandlingCallback error_handler,
      KeyHashingCallback key_hasher,
      NewUserAuthSessionCallback new_user_flow,
      ExistingUserAuthSessionCallback existing_user_flow,
      std::unique_ptr<UserContext> context,
      absl::optional<user_data_auth::StartAuthSessionReply> reply);

  // Authenticates AuthSession with data in `context` and passes parameters to
  // `OnAuthenticateSessionGeneric`.
  // Key in `context` should be already hashed.
  void AuthenticateSessionGeneric(ErrorHandlingCallback auth_error_callback,
                                  TransformCryotohomeKeyCallback transformer,
                                  ContextCallback continuation,
                                  std::unique_ptr<UserContext> context);

  // AuthSession authentication callback: performs error handling and
  // passes control to `continuation`.
  void OnAuthenticateSessionGeneric(
      ErrorHandlingCallback error_callback,
      ContextCallback continuation,
      std::unique_ptr<UserContext> context,
      absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply);

  // Adds initial credentials for the new user, and passes parameters to
  // `OnAddInitialCredentialsGeneric`.
  // Key in `context` should be already hashed.
  void AddInitialCredentialsGeneric(ErrorHandlingCallback error_handler,
                                    TransformCryotohomeKeyCallback transformer,
                                    ContextCallback continuation,
                                    std::unique_ptr<UserContext> context);

  // AuthSession callback invoked after adding initial credential for the new
  // user.
  // Performs error handling and passes control to `continuation`.
  void OnAddInitialCredentialsGeneric(
      ErrorHandlingCallback error_handler,
      ContextCallback continuation,
      std::unique_ptr<UserContext> user_context,
      absl::optional<user_data_auth::AddCredentialsReply> reply);

  // Attempts to perform a mount (with mount request additionally adjusted
  // by `configurator`) and invokes `continuation` upon success.
  // Note: it can be used as `NewUserAuthSessionCallback`/
  // `ExistingUserAuthSessionCallback` when first parameters are bound.
  void MountGeneric(ErrorHandlingCallback error_handler,
                    ConfigureMountCallback configurator,
                    ContextCallback continuation,
                    std::unique_ptr<UserContext> context);

  // Mount callback: Performs error handling and passes control to
  // `continuation`.
  void OnMountGeneric(ErrorHandlingCallback error_handler,
                      ContextCallback continuation,
                      std::unique_ptr<UserContext> user_context,
                      absl::optional<user_data_auth::MountReply> reply);

  void UnmountGeneric(ErrorHandlingCallback error_handler,
                      ContextCallback continuation,
                      std::unique_ptr<UserContext> context);

  void OnUnmountGeneric(ErrorHandlingCallback error_handler,
                        ContextCallback continuation,
                        std::unique_ptr<UserContext> context,
                        absl::optional<user_data_auth::UnmountReply> reply);

  // Attempts to remove user home directory, and invokes `continuation`
  // upon success.
  void RemoveGeneric(ErrorHandlingCallback error_handler,
                     ContextCallback continuation,
                     std::unique_ptr<UserContext> context);

  // Internal callback for Remove call.
  void OnRemoveGeneric(ErrorHandlingCallback error_handler,
                       ContextCallback continuation,
                       std::unique_ptr<UserContext> context,
                       absl::optional<user_data_auth::RemoveReply> reply);

  // Attempts to update user's password and calls `continuation` upon success.
  // It is assumed that `context` has new password stored as `replacement key`,
  // and authentication key as a regular `key`.
  void UpdateCredentialsGeneric(ErrorHandlingCallback error_handler,
                                ContextCallback continuation,
                                std::unique_ptr<UserContext> context);

  // Internal callback for UpdateCredentials call.
  void OnUpdateCredentialsGeneric(
      ErrorHandlingCallback error_handler,
      ContextCallback continuation,
      std::unique_ptr<UserContext> context,
      absl::optional<user_data_auth::UpdateCredentialReply> reply);

  // Common part of login logic shared by user creation flow and flow when
  // user have changed password elsewhere and decides to re-create cryptohome.
  void CompleteLoginImpl(std::unique_ptr<UserContext> user_context);
  void LoginAsKioskImpl(const AccountId& app_account_id,
                        user_manager::UserType user_type,
                        bool force_dircrypto);

  void PrepareForNewAttempt(const std::string& method_id,
                            const std::string& long_desc);

  // Simple callback that notifies about mount success / failure.
  void NotifyAuthSuccess(std::unique_ptr<UserContext> context);
  void NotifyGuestSuccess(std::unique_ptr<UserContext> context);
  void NotifyFailure(AuthFailure::FailureReason reason,
                     std::unique_ptr<UserContext> context);

  // Handles errors specific to authenticating existing users with the password:
  //   if password is known to be correct (e.g. it comes from online auth flow),
  //   special error code would be raised in case of "incorrect password" to
  //   indicate a need to replace password.
  // Other errors are handled by `fallback`.
  void ExistingUserPasswordAuthenticationErrorHandling(
      ErrorHandlingCallback fallback,
      bool verified_password,
      std::unique_ptr<UserContext> user_context,
      user_data_auth::CryptohomeErrorCode error);

  void NonOwnerUnmountErrorHandler(std::unique_ptr<UserContext> context,
                                   user_data_auth::CryptohomeErrorCode error);

  // Handles errors specific to Mounting, e.g. required migration.
  // Other errors are handled by `fallback`.
  void MountErrorHandling(ErrorHandlingCallback fallback,
                          std::unique_ptr<UserContext> user_context,
                          user_data_auth::CryptohomeErrorCode error);

  // Generic error handler, can be used as ErrorHandlingCallback when first
  // parameter is bound to a user type-specific failure reason.
  void ProcessCryptohomeError(AuthFailure::FailureReason default_reason,
                              std::unique_ptr<UserContext> user_context,
                              user_data_auth::CryptohomeErrorCode error);

  // Callback used for new regular users - would fail iff device is
  // running in Safe mode (as new users can not be owners).
  void FailIfInSafeMode(ContextCallback continuation,
                        std::unique_ptr<UserContext> context);

  // Callback used for existing regular users - in safe mode would check
  // if home directory contains valid owner key. If key is not found,
  // would unmount directory and notify failure.
  void RunSafeModeChecks(ContextCallback continuation,
                         std::unique_ptr<UserContext> context);
  void OnOwnershipCheckedForSafeMode(ContextCallback continuation,
                                     std::unique_ptr<UserContext> context,
                                     bool is_owner);

  void OnUnmountOwnerRequired(
      std::unique_ptr<UserContext> user_context,
      absl::optional<user_data_auth::UnmountReply> reply);

  const bool is_ephemeral_mount_enforced_;
  std::unique_ptr<SafeModeDelegate> safe_mode_delegate_;

  base::WeakPtrFactory<AuthSessionAuthenticator> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_AUTH_SESSION_AUTHENTICATOR_H_
