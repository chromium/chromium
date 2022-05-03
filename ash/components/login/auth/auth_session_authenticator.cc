// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/auth_session_authenticator.h"

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/components/cryptohome/cryptohome_util.h"
#include "ash/components/cryptohome/system_salt_getter.h"
#include "ash/components/cryptohome/userdataauth_util.h"
#include "ash/components/login/auth/cryptohome_key_constants.h"
#include "ash/components/login/auth/cryptohome_parameter_utils.h"
#include "ash/components/login/auth/operation_chain_runner.h"
#include "ash/components/login/auth/user_context.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/notreached.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_names.h"

namespace ash {

AuthSessionAuthenticator::AuthSessionAuthenticator(
    AuthStatusConsumer* consumer,
    std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
    bool is_ephemeral_mount_enforced)
    : Authenticator(consumer),
      is_ephemeral_mount_enforced_(is_ephemeral_mount_enforced),
      safe_mode_delegate_(std::move(safe_mode_delegate)),
      auth_factor_editor_(std::make_unique<AuthFactorEditor>()),
      auth_performer_(std::make_unique<AuthPerformer>()),
      mount_performer_(std::make_unique<MountPerformer>()) {}

AuthSessionAuthenticator::~AuthSessionAuthenticator() = default;

// Completes online authentication:
// *  User is likely to be new
// *  Provided password is assumed to be just verified by online flow
// This method is also called in case of password change detection if user
// decides to remove old cryptohome and start anew, which can only happen
// as a result of prior CompleteLogin call.
void AuthSessionAuthenticator::CompleteLogin(
    std::unique_ptr<UserContext> user_context) {
  PrepareForNewAttempt("CompleteLogin", "Regular user after online sign-in");
  CompleteLoginImpl(std::move(user_context));
}

// Implementation part, shared by CompleteLogin and ResyncEncryptedData.
void AuthSessionAuthenticator::CompleteLoginImpl(
    std::unique_ptr<UserContext> context) {
  DCHECK(context);
  DCHECK(context->GetUserType() == user_manager::USER_TYPE_REGULAR ||
         context->GetUserType() == user_manager::USER_TYPE_CHILD ||
         context->GetUserType() == user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  // For now we don't support empty passwords:
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    if (context->GetKey()->GetSecret().empty()) {
      NOTIMPLEMENTED();
      NotifyFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME,
                    std::move(context));
      return;
    }
  }
  auth_performer_->StartAuthSession(
      std::move(context), is_ephemeral_mount_enforced_,
      base::BindOnce(&AuthSessionAuthenticator::DoCompleteLogin,
                     weak_factory_.GetWeakPtr()));
}

void AuthSessionAuthenticator::DoCompleteLogin(
    bool user_exists,
    std::unique_ptr<UserContext> context,
    absl::optional<CryptohomeError> error) {
  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      is_ephemeral_mount_enforced_ ? AuthFailure::COULD_NOT_MOUNT_TMPFS
                                   : AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for Regular user "
                     << error.value().error_code;
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  LOGIN_LOG(EVENT) << "Regular user CompleteLogin " << user_exists;
  if (user_exists && is_ephemeral_mount_enforced_) {  // Should not happen
    // If ephemeral mount is enforced, cryptohomed should delete all existing
    // home directories before handling any requests.
    LOGIN_LOG(ERROR) << "Home directory exists for ephemeral user session";
    NotifyFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS, std::move(context));
    return;
  }
  std::vector<AuthOperation> steps;
  if (!user_exists) {
    if (safe_mode_delegate_->IsSafeMode()) {
      LOGIN_LOG(ERROR) << "New users are not allowed in safe mode";
      NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
      return;
    }

    if (is_ephemeral_mount_enforced_) {  // New ephemeral user
      steps.push_back(base::BindOnce(&MountPerformer::MountEphemeralDirectory,
                                     mount_performer_->AsWeakPtr()));
      steps.push_back(base::BindOnce(&AuthFactorEditor::AddContextKey,
                                     auth_factor_editor_->AsWeakPtr()));
    } else {  // New persistent user
      steps.push_back(base::BindOnce(&MountPerformer::CreateNewUser,
                                     mount_performer_->AsWeakPtr()));
      steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                     mount_performer_->AsWeakPtr()));
      steps.push_back(base::BindOnce(&AuthFactorEditor::AddContextKey,
                                     auth_factor_editor_->AsWeakPtr()));
    }
  } else {  // existing user
    // We are sure that password is correct, so intercept authentication failure
    // events and treat them as password change signals.
    error_callback =
        base::BindOnce(&AuthSessionAuthenticator::HandlePasswordChangeDetected,
                       weak_factory_.GetWeakPtr(), std::move(error_callback));
    // Existing users might require encryption migration: intercept related
    // error codes as well.
    error_callback =
        base::BindOnce(&AuthSessionAuthenticator::HandleMigrationRequired,
                       weak_factory_.GetWeakPtr(), std::move(error_callback));

    steps.push_back(base::BindOnce(&AuthPerformer::AuthenticateUsingKey,
                                   auth_performer_->AsWeakPtr()));
    steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                   mount_performer_->AsWeakPtr()));
    if (safe_mode_delegate_->IsSafeMode()) {
      steps.push_back(
          base::BindOnce(&AuthSessionAuthenticator::CheckOwnershipOperation,
                         weak_factory_.GetWeakPtr()));
    }
  }
  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

// Authentication from user pod.
// *  User could mistype their password/PIN.
// *  User homedir is expected to exist
// *  Should not be used when ephemeral login is enforced.
void AuthSessionAuthenticator::AuthenticateToLogin(
    std::unique_ptr<UserContext> context) {
  DCHECK(context);
  DCHECK(context->GetUserType() == user_manager::USER_TYPE_REGULAR ||
         context->GetUserType() == user_manager::USER_TYPE_CHILD ||
         context->GetUserType() == user_manager::USER_TYPE_ACTIVE_DIRECTORY);

  PrepareForNewAttempt("AuthenticateToLogin", "Returning regular user");

  // For now we don't support empty passwords:
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    if (context->GetKey()->GetSecret().empty()) {
      NOTIMPLEMENTED();
      NotifyFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME,
                    std::move(context));
      return;
    }
  }
  auth_performer_->StartAuthSession(
      std::move(context), is_ephemeral_mount_enforced_,
      base::BindOnce(&AuthSessionAuthenticator::DoLoginAsExistingUser,
                     weak_factory_.GetWeakPtr()));
}

void AuthSessionAuthenticator::DoLoginAsExistingUser(
    bool user_exists,
    std::unique_ptr<UserContext> context,
    absl::optional<CryptohomeError> error) {
  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      is_ephemeral_mount_enforced_ ? AuthFailure::COULD_NOT_MOUNT_TMPFS
                                   : AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for Regular user "
                     << error.value().error_code;
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  LOGIN_LOG(EVENT) << "Regular user login " << user_exists;
  if (user_exists && is_ephemeral_mount_enforced_) {  // Should not happen
    LOGIN_LOG(ERROR) << "Home directory exists for ephemeral user session";
    NotifyFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS, std::move(context));
    return;
  }
  if (!user_exists) {  // Should not happen
    LOGIN_LOG(ERROR)
        << "User directory does not exist for supposedly existing user";
    std::move(error_callback)
        .Run(std::move(context),
             CryptohomeError{
                 user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND});
    return;
  }
  DCHECK(user_exists && !is_ephemeral_mount_enforced_);

  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  // Existing users might require encryption migration: intercept related
  // error codes.
  error_callback =
      base::BindOnce(&AuthSessionAuthenticator::HandleMigrationRequired,
                     weak_factory_.GetWeakPtr(), std::move(error_callback));

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&AuthPerformer::AuthenticateUsingKey,
                                 auth_performer_->AsWeakPtr()));
  // TODO(antrim): Check for migration
  steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                 mount_performer_->AsWeakPtr()));
  if (safe_mode_delegate_->IsSafeMode()) {
    steps.push_back(
        base::BindOnce(&AuthSessionAuthenticator::CheckOwnershipOperation,
                       weak_factory_.GetWeakPtr()));
  }
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::LoginOffTheRecord() {
  PrepareForNewAttempt("LoginOffTheRecord", "Guest login");

  std::unique_ptr<UserContext> context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_GUEST, user_manager::GuestAccountId());

  // Guest can not be be an owner.
  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Guest can not sign-in in safe mode";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }

  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(), AuthFailure::COULD_NOT_MOUNT_TMPFS);

  AuthSuccessCallback success_callback =
      base::BindOnce(&AuthSessionAuthenticator::NotifyGuestSuccess,
                     weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&MountPerformer::MountGuestDirectory,
                                 mount_performer_->AsWeakPtr()));
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

// Public sessions aka Managed Guest Sessions are always ephemeral.
// Most of the MGS have no credentials, but it optionally can
// have a password set by extension (so that it is possible to lock session).
void AuthSessionAuthenticator::LoginAsPublicSession(
    const UserContext& user_context) {
  DCHECK_EQ(user_context.GetUserType(), user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  PrepareForNewAttempt("LoginAsPublicSession", "Managed guest session");

  std::unique_ptr<UserContext> context =
      std::make_unique<UserContext>(user_context);

  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Managed guests can not sign-in in safe mode";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }

  auth_performer_->StartAuthSession(
      std::move(context), true /* ephemeral */,
      base::BindOnce(&AuthSessionAuthenticator::DoLoginAsPublicSession,
                     weak_factory_.GetWeakPtr()));
}

void AuthSessionAuthenticator::DoLoginAsPublicSession(
    bool user_exists,
    std::unique_ptr<UserContext> context,
    absl::optional<CryptohomeError> error) {
  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(), AuthFailure::COULD_NOT_MOUNT_TMPFS);

  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for MGS "
                     << error.value().error_code;
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  if (user_exists) {  // Should not happen
    LOGIN_LOG(ERROR) << "Home directory exists for MGS";
    NotifyFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS, std::move(context));
    return;
  }
  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&MountPerformer::MountEphemeralDirectory,
                                 mount_performer_->AsWeakPtr()));
  if (context->GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN ||
      !context->GetKey()->GetSecret().empty()) {
    steps.push_back(base::BindOnce(&AuthFactorEditor::AddContextKey,
                                   auth_factor_editor_->AsWeakPtr()));
  }

  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::LoginAsKioskAccount(
    const AccountId& app_account_id) {
  LoginAsKioskImpl(app_account_id, user_manager::USER_TYPE_KIOSK_APP,
                   /*force_dircrypto=*/false);
}

void AuthSessionAuthenticator::LoginAsArcKioskAccount(
    const AccountId& app_account_id) {
  LoginAsKioskImpl(app_account_id, user_manager::USER_TYPE_ARC_KIOSK_APP,
                   /*force_dircrypto=*/true);
}

void AuthSessionAuthenticator::LoginAsWebKioskAccount(
    const AccountId& app_account_id) {
  LoginAsKioskImpl(app_account_id, user_manager::USER_TYPE_WEB_KIOSK_APP,
                   /*force_dircrypto=*/false);
}

void AuthSessionAuthenticator::LoginAsKioskImpl(
    const AccountId& app_account_id,
    user_manager::UserType user_type,
    bool force_dircrypto) {
  PrepareForNewAttempt("LoginAs*Kiosk", "Kiosk user");

  std::unique_ptr<UserContext> context =
      std::make_unique<UserContext>(user_type, app_account_id);
  context->SetIsForcingDircrypto(force_dircrypto);

  // Kiosk can not be an owner.
  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Kiosks can not sign-in in safe mode";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }
  auth_performer_->StartAuthSession(
      std::move(context), is_ephemeral_mount_enforced_,
      base::BindOnce(&AuthSessionAuthenticator::DoLoginAsKiosk,
                     weak_factory_.GetWeakPtr()));
}

void AuthSessionAuthenticator::DoLoginAsKiosk(
    bool user_exists,
    std::unique_ptr<UserContext> context,
    absl::optional<CryptohomeError> error) {
  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      is_ephemeral_mount_enforced_ ? AuthFailure::COULD_NOT_MOUNT_TMPFS
                                   : AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for Kiosk "
                     << error.value().error_code;
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  LOGIN_LOG(EVENT) << "Kiosk user " << user_exists;
  if (user_exists && is_ephemeral_mount_enforced_) {  // Should not happen
    LOGIN_LOG(ERROR) << "Home directory exists for ephemeral Kiosk session";
    NotifyFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS, std::move(context));
    return;
  }
  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  if (user_exists) {
    // Existing Kiosks might require encryption migration: intercept related
    // error codes.
    error_callback =
        base::BindOnce(&AuthSessionAuthenticator::HandleMigrationRequired,
                       weak_factory_.GetWeakPtr(), std::move(error_callback));

    steps.push_back(base::BindOnce(&AuthPerformer::AuthenticateAsKiosk,
                                   auth_performer_->AsWeakPtr()));
    steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                   mount_performer_->AsWeakPtr()));
  } else if (is_ephemeral_mount_enforced_) {
    steps.push_back(base::BindOnce(&MountPerformer::MountEphemeralDirectory,
                                   mount_performer_->AsWeakPtr()));
  } else {
    steps.push_back(base::BindOnce(&MountPerformer::CreateNewUser,
                                   mount_performer_->AsWeakPtr()));
    steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                   mount_performer_->AsWeakPtr()));
    steps.push_back(base::BindOnce(&AuthFactorEditor::AddKioskKey,
                                   auth_factor_editor_->AsWeakPtr()));
  }
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::OnAuthSuccess() {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::OnAuthFailure(const AuthFailure& error) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::RecoverEncryptedData(
    std::unique_ptr<UserContext> context,
    const std::string& old_password) {
  DCHECK(context);
  DCHECK(!context->GetAuthSessionId().empty());
  DCHECK(!is_ephemeral_mount_enforced_);
  LOGIN_LOG(USER) << "Attempting to update user password";

  const cryptohome::KeyDefinition* password_key_def =
      context->GetAuthFactorsData().FindOnlinePasswordKey();
  DCHECK(password_key_def);
  const std::string key_label = password_key_def->label;

  if (!context->HasReplacementKey()) {
    // Assume that there was an attempt to use the key, so it is was already
    // hashed.
    DCHECK(context->GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN);
    // Make sure that the key has correct label.
    context->GetKey()->SetLabel(key_label);
    context->SaveKeyForReplacement();
  }

  chromeos::Key auth_key(old_password);
  auth_key.SetLabel(key_label);
  context->SetKey(auth_key);

  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(), AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);

  // Existing users might require encryption migration: intercept related
  // error codes.
  error_callback =
      base::BindOnce(&AuthSessionAuthenticator::HandleMigrationRequired,
                     weak_factory_.GetWeakPtr(), std::move(error_callback));

  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&AuthPerformer::AuthenticateUsingKey,
                                 auth_performer_->AsWeakPtr()));
  steps.push_back(base::BindOnce(&AuthFactorEditor::ReplaceContextKey,
                                 auth_factor_editor_->AsWeakPtr()));
  steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                 mount_performer_->AsWeakPtr()));
  if (safe_mode_delegate_->IsSafeMode()) {
    steps.push_back(
        base::BindOnce(&AuthSessionAuthenticator::CheckOwnershipOperation,
                       weak_factory_.GetWeakPtr()));
  }
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::ResyncEncryptedData(
    std::unique_ptr<UserContext> context) {
  LOGIN_LOG(USER) << "Re-create cryptohome";

  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(), AuthFailure::DATA_REMOVAL_FAILED);

  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::CompleteLoginImpl, weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&MountPerformer::RemoveUserDirectory,
                                 mount_performer_->AsWeakPtr()));
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::PrepareForNewAttempt(
    const std::string& method_id,
    const std::string& long_desc) {
  LOGIN_LOG(USER) << "Authentication attempt : " << long_desc;
  VLOG(1) << "AuthSessionAuthenticator::" << method_id;

  // Assume no ongoing authentication requests happen at the moment.
  DCHECK(!weak_factory_.HasWeakPtrs());
  // Clear all ongoing requests
  auth_factor_editor_->InvalidateCurrentAttempts();
  auth_performer_->InvalidateCurrentAttempts();
  mount_performer_->InvalidateCurrentAttempts();
  weak_factory_.InvalidateWeakPtrs();
}

bool AuthSessionAuthenticator::ResolveCryptohomeError(
    AuthFailure::FailureReason default_error,
    CryptohomeError& error) {
  switch (error.error_code) {
    // Not an error:
    case user_data_auth::CRYPTOHOME_ERROR_NOT_SET:
    // Some errors need to be handled explicitly and can not be resolved to
    // AuthFailure:
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION:
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE:
    case user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED:
    // Fatal errors that can not be handled gracefully:
    case user_data_auth::CRYPTOHOME_ERROR_LOCKBOX_SIGNATURE_INVALID:
    case user_data_auth::CRYPTOHOME_ERROR_LOCKBOX_CANNOT_SIGN:
    case user_data_auth::CRYPTOHOME_ERROR_BOOT_ATTRIBUTE_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_BOOT_ATTRIBUTES_CANNOT_SIGN:
    case user_data_auth::CRYPTOHOME_ERROR_TPM_EK_NOT_AVAILABLE:
    case user_data_auth::CRYPTOHOME_ERROR_ATTESTATION_NOT_READY:
    case user_data_auth::CRYPTOHOME_ERROR_CANNOT_CONNECT_TO_CA:
    case user_data_auth::CRYPTOHOME_ERROR_CA_REFUSED_ENROLLMENT:
    case user_data_auth::CRYPTOHOME_ERROR_CA_REFUSED_CERTIFICATE:
    case user_data_auth::CRYPTOHOME_ERROR_INTERNAL_ATTESTATION_ERROR:
    case user_data_auth::
        CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_INVALID:
    case user_data_auth::
        CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_STORE:
    case user_data_auth::
        CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_REMOVE:
    case user_data_auth::CRYPTOHOME_ERROR_UPDATE_USER_ACTIVITY_TIMESTAMP_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_FAILED_TO_EXTEND_PCR:
    case user_data_auth::CRYPTOHOME_ERROR_FAILED_TO_READ_PCR:
    case user_data_auth::CRYPTOHOME_ERROR_PCR_ALREADY_EXTENDED:
    case user_data_auth::CRYPTOHOME_ERROR_FIDO_MAKE_CREDENTIAL_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_FIDO_GET_ASSERTION_FAILED:
      return false;

    case user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND:
      error.failure_reason = AuthFailure::MISSING_CRYPTOHOME;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_NOT_IMPLEMENTED:
    case user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT:
    case user_data_auth::CRYPTOHOME_TOKEN_SERIALIZATION_FAILED:
      // Fatal implementation errors
      error.failure_reason = default_error;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_ERROR_INTERNAL:
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED:
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_DENIED:
      // Fingerprint errors
      error.failure_reason = default_error;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_FATAL:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_QUOTA_EXCEEDED:
    case user_data_auth::CRYPTOHOME_ERROR_BACKING_STORE_FAILURE:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_FINALIZE_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_GET_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_SET_FAILED:
      // Fatal system state errors
      error.failure_reason = default_error;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_MIGRATE_KEY_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED:

    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_LABEL_EXISTS:
    case user_data_auth::CRYPTOHOME_ERROR_UPDATE_SIGNATURE_INVALID:
      // Assumptions about key are not correct
      error.failure_reason = default_error;
      break;
    case user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN:
      // Auth session expired, might need to handle it separately later.
      error.failure_reason = default_error;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_COMM_ERROR:
    case user_data_auth::CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT:
    case user_data_auth::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK:
      error.failure_reason = AuthFailure::TPM_ERROR;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY:
      // Assumption about system state is not correct
      error.failure_reason = default_error;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_REMOVE_FAILED:
      error.failure_reason = AuthFailure::DATA_REMOVAL_FAILED;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_UPDATE_REQUIRED:
      error.failure_reason = AuthFailure::TPM_UPDATE_REQUIRED;
      break;
    case user_data_auth::CRYPTOHOME_ERROR_VAULT_UNRECOVERABLE:
      error.failure_reason = AuthFailure::UNRECOVERABLE_CRYPTOHOME;
      break;
    case user_data_auth::CryptohomeErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case user_data_auth::CryptohomeErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_:
      // Ignored
      break;
    default:
      // We need the default case here so that it is possible to add new
      // CryptohomeErrorCode, because CryptohomeErrorCode is defined in another
      // repo.
      // However, we should seek to handle all CryptohomeErrorCode and not let
      // any of them hit the default block.
      NOTREACHED() << "Unhandled CryptohomeErrorCode in ProcessCryptohomeError"
                      ": "
                   << static_cast<int>(error.error_code);
      return false;
  }
  return true;
}

void AuthSessionAuthenticator::ProcessCryptohomeError(
    AuthFailure::FailureReason default_error,
    std::unique_ptr<UserContext> context,
    CryptohomeError error) {
  if (!consumer_)
    return;
  DCHECK_NE(error.error_code, user_data_auth::CRYPTOHOME_ERROR_NOT_SET);

  if (error.error_code == user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED) {
    // for now treat it as login failed:
    error.failure_reason = default_error;
    NotifyFailure(error.failure_reason, std::move(context));
    return;
  }
  bool handled = ResolveCryptohomeError(default_error, error);
  CHECK(handled);
  NotifyFailure(error.failure_reason, std::move(context));
}

void AuthSessionAuthenticator::HandlePasswordChangeDetected(
    AuthErrorCallback fallback,
    std::unique_ptr<UserContext> context,
    CryptohomeError error) {
  if (error.error_code ==
      user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED) {
    LOGIN_LOG(EVENT) << "Password change detected";
    if (!consumer_)
      return;
    consumer_->OnPasswordChangeDetected(*context);
    return;
  }
  std::move(fallback).Run(std::move(context), std::move(error));
}

void AuthSessionAuthenticator::HandleMigrationRequired(
    AuthErrorCallback fallback,
    std::unique_ptr<UserContext> context,
    CryptohomeError error) {
  const bool migration_required =
      error.error_code == user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION;
  const bool incomplete_migration =
      error.error_code ==
      user_data_auth::CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE;
  if (migration_required || incomplete_migration) {
    LOGIN_LOG(EVENT) << "Old encryption detected";
    if (!consumer_)
      return;
    consumer_->OnOldEncryptionDetected(*context, incomplete_migration);
    return;
  }
  std::move(fallback).Run(std::move(context), std::move(error));
}

void AuthSessionAuthenticator::NotifyAuthSuccess(
    std::unique_ptr<UserContext> context) {
  LOGIN_LOG(EVENT) << "Logged in successfully";
  if (consumer_)
    consumer_->OnAuthSuccess(*context);
}

void AuthSessionAuthenticator::NotifyGuestSuccess(
    std::unique_ptr<UserContext> context) {
  LOGIN_LOG(EVENT) << "Logged in as guest";
  if (consumer_)
    consumer_->OnOffTheRecordAuthSuccess();
}

void AuthSessionAuthenticator::NotifyFailure(
    AuthFailure::FailureReason reason,
    std::unique_ptr<UserContext> context) {
  if (consumer_)
    consumer_->OnAuthFailure(AuthFailure(reason));
}

void AuthSessionAuthenticator::CheckOwnershipOperation(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (!safe_mode_delegate_->IsSafeMode()) {
    std::move(callback).Run(std::move(context), absl::nullopt);
    return;
  }
  LOGIN_LOG(EVENT) << "Running in safe mode";
  // Save value as context will be moved.
  auto user_hash = context->GetUserIDHash();
  // Device is running in the safe mode, need to check if user is an owner.
  safe_mode_delegate_->CheckSafeModeOwnership(
      user_hash,
      base::BindOnce(&AuthSessionAuthenticator::OnSafeModeOwnershipCheck,
                     weak_factory_.GetWeakPtr(), std::move(context),
                     std::move(callback)));
}

void AuthSessionAuthenticator::OnSafeModeOwnershipCheck(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    bool is_owner) {
  if (is_owner) {
    LOGIN_LOG(EVENT) << "Safe mode: owner";
    std::move(callback).Run(std::move(context), absl::nullopt);
    return;
  }
  LOGIN_LOG(EVENT) << "Safe mode: non-owner";
  mount_performer_->UnmountDirectories(
      std::move(context),
      base::BindOnce(&AuthSessionAuthenticator::OnUnmountForNonOwner,
                     weak_factory_.GetWeakPtr()));
}

// Notify that owner is required upon success
// Crash if directory could not be unmounted
void AuthSessionAuthenticator::OnUnmountForNonOwner(
    std::unique_ptr<UserContext> context,
    absl::optional<CryptohomeError> error) {
  if (error) {
    // Crash if could not unmount home directory, and let session_manager
    // handle it.
    LOG(FATAL) << "Failed to unmount non-owner home directory "
               << error->error_code;
  } else {
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
  }
}

}  // namespace ash
