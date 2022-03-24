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

namespace {

// -- TransformCryptohomeKeyCallback implementations
void TransformToLabeledKey(const UserContext& context, cryptohome::Key* key) {
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateKeyDefFromUserContext(context), key);
}

void TransformToWildcardKey(const UserContext& context, cryptohome::Key* key) {
  cryptohome::KeyDefinitionToKey(
      cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
          context),
      key);
}

void CreateKioskKey(const UserContext& context, cryptohome::Key* key) {
  cryptohome::KeyData* data = key->mutable_data();
  data->set_label(kCryptohomePublicMountLabel);
  data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
}

void CreateKioskWildcardKey(const UserContext& context, cryptohome::Key* key) {
  cryptohome::KeyData* data = key->mutable_data();
  // Do not set label
  data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
}

// -- ConfigureAuthSessionCallback implementations

user_data_auth::StartAuthSessionRequest ConfigureRegularSession(
    const UserContext& context,
    user_data_auth::StartAuthSessionRequest request) {
  // TODO(crbug.com/1288835): Once ephemeral policy would be handled on Chrome
  // side update flags.
  request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_NONE);
  return request;
}

user_data_auth::StartAuthSessionRequest ConfigureEphemeralSession(
    const UserContext& context,
    user_data_auth::StartAuthSessionRequest request) {
  request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER);
  return request;
}

// -- ConfigureMountCallback implementations

user_data_auth::MountRequest ConfigureGenericMount(
    const UserContext& context,
    user_data_auth::MountRequest request) {
  if (context.IsForcingDircrypto())
    request.set_force_dircrypto_if_available(true);
  return request;
}

user_data_auth::MountRequest ConfigureGuestMount(
    const UserContext& context,
    user_data_auth::MountRequest request) {
  request.set_guest_mount(true);
  return request;
}

user_data_auth::MountRequest ConfigureKioskMount(
    const UserContext& context,
    user_data_auth::MountRequest request) {
  request.set_public_mount(true);
  return ConfigureGenericMount(context, std::move(request));
}

// -- KeyHashingCallback implementations

void IgnoreHashing(
    std::unique_ptr<UserContext> context,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> consumer) {
  std::move(consumer).Run(std::move(context));
}

void TransformGaiaPasswordWithSalt(
    std::unique_ptr<UserContext> user_context,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> callback,
    const std::string& system_salt) {
  DCHECK_EQ(user_context->GetKey()->GetKeyType(), Key::KEY_TYPE_PASSWORD_PLAIN);
  user_context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                    system_salt);
  std::move(callback).Run(std::move(user_context));
}

void HashPassword(
    std::unique_ptr<UserContext> context,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> consumer) {
  if (context->GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN) {
    std::move(consumer).Run(std::move(context));
    return;
  }

  DCHECK(!context->IsUsingPin());
  // TODO(antrim): use key metadata if necessary.
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      TransformGaiaPasswordWithSalt, std::move(context), std::move(consumer)));
}

}  // namespace

AuthSessionAuthenticator::AuthSessionAuthenticator(
    AuthStatusConsumer* consumer,
    std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
    bool is_ephemeral_mount_enforced)
    : Authenticator(consumer),
      is_ephemeral_mount_enforced_(is_ephemeral_mount_enforced),
      safe_mode_delegate_(std::move(safe_mode_delegate)) {}

AuthSessionAuthenticator::~AuthSessionAuthenticator() = default;

// Completes online authentication:
// *  User is likely to be new
// *  Provided password is assumed to be just verified by online flow
// This method is also called in case of password change detection if user
// decides to remove old cryptohome and start anew, which can only happen
// as a result of prior CompleteLogin call.
void AuthSessionAuthenticator::CompleteLogin(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  DCHECK(user_context->GetUserType() == user_manager::USER_TYPE_REGULAR ||
         user_context->GetUserType() == user_manager::USER_TYPE_CHILD ||
         user_context->GetUserType() ==
             user_manager::USER_TYPE_ACTIVE_DIRECTORY);

  PrepareForNewAttempt("CompleteLogin", "Regular user after online sign-in");
  CompleteLoginImpl(std::move(user_context));
}

// Implementation part, shared by CompleteLogin and ResyncEncryptedData.
void AuthSessionAuthenticator::CompleteLoginImpl(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  DCHECK(user_context->GetUserType() == user_manager::USER_TYPE_REGULAR ||
         user_context->GetUserType() == user_manager::USER_TYPE_CHILD ||
         user_context->GetUserType() ==
             user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  // For now we don't support empty passwords:
  if (user_context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    if (user_context->GetKey()->GetSecret().empty()) {
      NOTIMPLEMENTED();
      if (consumer_)
        consumer_->OnAuthFailure(
            AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
      return;
    }
  }

  // (1) Initialize AuthSession & transform keys
  //   (1.1) For regular users
  //   (1.2) Key is a password
  //   For existing users:
  //     (2) Authenticate AuthSession
  //       (2.1) Password mismatch means that password changed
  //     (3) Mount directory
  //     (4) (Safe mode) Check ownership
  //     (#) Notify success
  //   For new users:
  //     (5) (Safe mode) fail, as this user can not be owner
  //     (6) Add user key
  //     (7) Authenticate session with same key
  //     (8) Mount home directory
  //     (#) Notify success
  // (*) Errors are notified as COULD_NOT_MOUNT_CRYPTOHOME

  // Callbacks are created in reverse order:

  // (*)
  auto error_handler_repeating = base::BindRepeating(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      /*default_reason=*/AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);

  // (#)
  auto success_split = base::SplitOnceCallback(
      base::BindOnce(&AuthSessionAuthenticator::NotifyAuthSuccess,
                     weak_factory_.GetWeakPtr()));
  // (8.1)
  ConfigureMountCallback mount_cfg = base::BindOnce(&ConfigureGenericMount);
  // (8)
  ContextCallback mount_create = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(mount_cfg),
      /*continuation=*/std::move(success_split.first));
  // (7)
  ContextCallback authenticate_same_key =
      base::BindOnce(&AuthSessionAuthenticator::AuthenticateSessionGeneric,
                     weak_factory_.GetWeakPtr(),
                     /*error_handler=*/base::BindOnce(error_handler_repeating),
                     /*transformer=*/base::BindOnce(&TransformToWildcardKey),
                     /*continuation=*/std::move(mount_create));

  // (6)
  ContextCallback add_credentials =
      base::BindOnce(&AuthSessionAuthenticator::AddInitialCredentialsGeneric,
                     weak_factory_.GetWeakPtr(),
                     /*error_handler=*/base::BindOnce(error_handler_repeating),
                     /*transformer=*/base::BindOnce(&TransformToLabeledKey),
                     /*continuation=*/std::move(authenticate_same_key));
  // (5)
  NewUserAuthSessionCallback new_user_flow = base::BindOnce(
      &AuthSessionAuthenticator::FailIfInSafeMode, weak_factory_.GetWeakPtr(),
      /*continuation=*/std::move(add_credentials));
  // (4)
  ContextCallback safe_mode_check = base::BindOnce(
      &AuthSessionAuthenticator::RunSafeModeChecks, weak_factory_.GetWeakPtr(),
      /*continuation=*/std::move(success_split.second));

  // (3.1)
  ConfigureMountCallback mount_regular_cfg =
      base::BindOnce(&ConfigureGenericMount);
  // (3)
  ContextCallback mount_existing = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(mount_regular_cfg),
      /*continuation=*/std::move(safe_mode_check));

  // (2.1)
  ErrorHandlingCallback auth_error_handler =
      base::BindOnce(&AuthSessionAuthenticator::
                         ExistingUserPasswordAuthenticationErrorHandling,
                     weak_factory_.GetWeakPtr(),
                     /*fallback=*/base::BindOnce(error_handler_repeating),
                     /*verified_password=*/true);
  // (2)
  ExistingUserAuthSessionCallback existing_user_flow = base::BindOnce(
      &AuthSessionAuthenticator::AuthenticateSessionGeneric,
      weak_factory_.GetWeakPtr(),
      /*error_handler=*/std::move(auth_error_handler),
      /*key_transformer=*/base::BindOnce(&TransformToWildcardKey),
      /*continuation=*/std::move(mount_existing));

  // (1.2)
  auto password_hasher = base::BindOnce(&HashPassword);
  // (1.1)
  auto regular_session_configurator = base::BindOnce(&ConfigureRegularSession);
  // (1)
  CreateAuthSessionGeneric(
      "RegularUser", base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(regular_session_configurator),
      /*key_hasher=*/std::move(password_hasher),
      /*new_user_flow=*/std::move(new_user_flow),
      /*existing_user_flow=*/std::move(existing_user_flow),
      std::move(user_context));
}

// Authentication from user pod.
// *  User could mistype their password/PIN.
// *  User homedir is expected to exist
void AuthSessionAuthenticator::AuthenticateToLogin(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  DCHECK(user_context->GetUserType() == user_manager::USER_TYPE_REGULAR ||
         user_context->GetUserType() == user_manager::USER_TYPE_CHILD ||
         user_context->GetUserType() ==
             user_manager::USER_TYPE_ACTIVE_DIRECTORY);

  PrepareForNewAttempt("AuthenticateToLogin", "Returning regular user");

  // For now we don't support empty passwords:
  if (user_context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    if (user_context->GetKey()->GetSecret().empty()) {
      NOTIMPLEMENTED();
      if (consumer_)
        consumer_->OnAuthFailure(
            AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
      return;
    }
  }

  // (1) Initialize AuthSession & transform keys
  //   (1.1) For regular users
  //   (1.2) Key is a password
  //   For existing users:
  //     (2) Authenticate AuthSession
  //     (3) Mount directory
  //         (3.1) with regular flags
  //     (4) (Safe mode) Check ownership
  //     (#) Notify success
  //   For new users:
  //     (5) Notify that cryptohome is missing
  // (*) Errors are notified as COULD_NOT_MOUNT_CRYPTOHOME

  // Callbacks are created in reverse order:

  // (*)
  auto error_handler_repeating = base::BindRepeating(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      /*default_reason=*/AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);

  // (#)
  ContextCallback success = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());
  // (5)
  NewUserAuthSessionCallback no_cryptohome = base::BindOnce(
      &AuthSessionAuthenticator::NotifyFailure, weak_factory_.GetWeakPtr(),
      AuthFailure::MISSING_CRYPTOHOME);
  // (4)
  ContextCallback safe_mode_check = base::BindOnce(
      &AuthSessionAuthenticator::RunSafeModeChecks, weak_factory_.GetWeakPtr(),
      /*continuation=*/std::move(success));
  // (3.1)
  ConfigureMountCallback mount_regular_cfg =
      base::BindOnce(&ConfigureGenericMount);
  // (3)
  ContextCallback mount_existing = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(mount_regular_cfg),
      /*continuation=*/std::move(safe_mode_check));
  // (2)
  ExistingUserAuthSessionCallback existing_user_flow = base::BindOnce(
      &AuthSessionAuthenticator::AuthenticateSessionGeneric,
      weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*key_transformer=*/base::BindOnce(&TransformToWildcardKey),
      /*continuation=*/std::move(mount_existing));
  // (1.2)
  auto password_hasher = base::BindOnce(&HashPassword);
  // (1.1)
  auto regular_session = base::BindOnce(&ConfigureRegularSession);
  // (1)
  CreateAuthSessionGeneric(
      "RegularReturningUser",
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(regular_session),
      /*key_hasher=*/std::move(password_hasher),
      /*new_user_flow=*/std::move(no_cryptohome),
      /*existing_user_flow=*/std::move(existing_user_flow),
      std::move(user_context));
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

  // (1) Initialize AuthSession & transform keys
  //   (1.1) Fake transformer
  //   (2) Guests mounts can not exist
  //   For new users:
  //     (3) Mount home directory
  //        (3.1) as a guest mount
  //     (#) Notify success
  // (*) Errors are notified as COULD_NOT_MOUNT_TMPFS
  // Callbacks are created in reverse order:

  // (*)
  auto error_handler_repeating = base::BindRepeating(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      /*default_reason=*/AuthFailure::COULD_NOT_MOUNT_TMPFS);
  // (#) Notify success
  ContextCallback success_callback =
      base::BindOnce(&AuthSessionAuthenticator::NotifyGuestSuccess,
                     weak_factory_.GetWeakPtr());
  // (3.1)
  ConfigureMountCallback guest_configurator =
      base::BindOnce(&ConfigureGuestMount);
  // (3)
  NewUserAuthSessionCallback guest_mount = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(guest_configurator),
      /*continuation=*/std::move(success_callback));
  // (2)
  ExistingUserAuthSessionCallback guest_exists = base::BindOnce(
      &AuthSessionAuthenticator::NotifyFailure, weak_factory_.GetWeakPtr(),
      AuthFailure::COULD_NOT_MOUNT_TMPFS);
  // (1.1) Fake transformer
  KeyHashingCallback password_hasher = base::BindOnce(&IgnoreHashing);
  // (1)
  CreateAuthSessionGeneric(
      "Guest", /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/base::BindOnce(&ConfigureEphemeralSession),
      /*key_hasher=*/std::move(password_hasher),
      /*new_user_flow=*/std::move(guest_mount),
      /*existing_user_flow=*/std::move(guest_exists), std::move(context));
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

  // (1) Initialize AuthSession & transform keys
  //   (1.1) MGS are always ephemeral
  //   (1.2) Key can be empty or be a password
  //   (2) MGS are ephemeral and can not exist
  //   For new users:
  //     (3) Add user key (even if it is empty, this is current limitation)
  //     (4) Authenticate session with same key
  //     (5) Mount home directory
  //     (#) Notify success
  // (*) Errors are notified as COULD_NOT_MOUNT_TMPFS

  // Callbacks are created in reverse order:

  // (*)
  auto error_handler_repeating = base::BindRepeating(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      /*default_reason=*/AuthFailure::COULD_NOT_MOUNT_TMPFS);
  // (#)
  ContextCallback success = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());
  // (5.1)
  ConfigureMountCallback mount_cfg = base::BindOnce(&ConfigureGenericMount);
  // (5)
  NewUserAuthSessionCallback mgs_mount = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(mount_cfg),
      /*continuation=*/std::move(success));
  // (4)
  ContextCallback authenticate_same_key =
      base::BindOnce(&AuthSessionAuthenticator::AuthenticateSessionGeneric,
                     weak_factory_.GetWeakPtr(),
                     /*error_handler=*/base::BindOnce(error_handler_repeating),
                     /*key_transformer=*/base::BindOnce(&TransformToLabeledKey),
                     /*continuation=*/std::move(mgs_mount));
  // (3)
  NewUserAuthSessionCallback new_user_flow =
      base::BindOnce(&AuthSessionAuthenticator::AddInitialCredentialsGeneric,
                     weak_factory_.GetWeakPtr(),
                     /*error_handler=*/base::BindOnce(error_handler_repeating),
                     /*key_transformer=*/base::BindOnce(&TransformToLabeledKey),
                     /*continuation=*/std::move(authenticate_same_key));
  // (2)
  ExistingUserAuthSessionCallback mgs_exists = base::BindOnce(
      &AuthSessionAuthenticator::NotifyFailure, weak_factory_.GetWeakPtr(),
      AuthFailure::COULD_NOT_MOUNT_TMPFS);
  // (1.2)
  auto password_hasher = base::BindOnce(&HashPassword);
  // (1.1)
  auto ephemeral_session_configurator =
      base::BindOnce(&ConfigureEphemeralSession);
  // (1)
  CreateAuthSessionGeneric(
      "ManagedGuest",
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(ephemeral_session_configurator),
      /*key_hasher=*/std::move(password_hasher),
      /*new_user_flow=*/std::move(new_user_flow),
      /*existing_user_flow=*/std::move(mgs_exists), std::move(context));
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

  // Guest can not be be an owner.
  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Kiosks can not sign-in in safe mode";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }

  // (1) Initialize AuthSession
  //   (1.1) For kiosks users
  //   (1.2) Ignore hashing, as there is no password
  //   For existing users:
  //     (2) Authenticate AuthSession
  //       (2.1) Use Kiosk-specific key w/o label
  //     (3) Mount directory
  //         (3.1) Usual mount (dircrypto enforcement is passed via context)
  //     (#) Notify success
  //   For new users:
  //     (4) Add user key
  //       (4.1) Use Kiosk-specific key
  //     (5) Authenticate session with same key
  //       (5.1) Use Kiosk-specific key
  //     (6) Mount home directory
  //        (6.1) with create request
  //     (#) Notify success
  // (*) Errors are notified as COULD_NOT_MOUNT_CRYPTOHOME

  // Callbacks are created in reverse order:

  // (*)
  auto error_handler_repeating = base::BindRepeating(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      /*default_reason=*/AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  // (#)
  auto success_split = base::SplitOnceCallback(
      base::BindOnce(&AuthSessionAuthenticator::NotifyAuthSuccess,
                     weak_factory_.GetWeakPtr()));
  // (6.1)
  ConfigureMountCallback mount_create_cfg =
      base::BindOnce(&ConfigureKioskMount);
  // (6)
  ContextCallback mount_create = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(mount_create_cfg),
      /*continuation=*/std::move(success_split.first));
  // (5)
  ContextCallback authenticate_same_key =
      base::BindOnce(&AuthSessionAuthenticator::AuthenticateSessionGeneric,
                     weak_factory_.GetWeakPtr(),
                     /*error_handler=*/base::BindOnce(error_handler_repeating),
                     /*key_transformer=*/base::BindOnce(&CreateKioskKey),
                     /*continuation=*/std::move(mount_create));
  // (4)
  NewUserAuthSessionCallback new_user_flow =
      base::BindOnce(&AuthSessionAuthenticator::AddInitialCredentialsGeneric,
                     weak_factory_.GetWeakPtr(),
                     /*error_handler=*/base::BindOnce(error_handler_repeating),
                     /*key_transformer=*/base::BindOnce(&CreateKioskKey),
                     /*continuation=*/std::move(authenticate_same_key));

  // (3.1)
  ConfigureMountCallback mount_kiosk_cfg = base::BindOnce(&ConfigureKioskMount);
  // (3)
  ContextCallback mount_existing = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(mount_kiosk_cfg),
      /*continuation=*/std::move(success_split.second));

  // (2)
  ExistingUserAuthSessionCallback existing_user_flow = base::BindOnce(
      &AuthSessionAuthenticator::AuthenticateSessionGeneric,
      weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*key_transformer=*/base::BindOnce(&CreateKioskWildcardKey),
      /*continuation=*/std::move(mount_existing));

  // (1.2)
  auto password_hasher = base::BindOnce(&IgnoreHashing);
  // (1.1)
  auto kiosk_session = base::BindOnce(&ConfigureRegularSession);
  // (1)
  CreateAuthSessionGeneric(
      "Kiosk", /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(kiosk_session),
      /*key_hasher=*/std::move(password_hasher),
      /*new_user_flow=*/std::move(new_user_flow),
      /*existing_user_flow=*/std::move(existing_user_flow), std::move(context));
}

void AuthSessionAuthenticator::OnAuthSuccess() {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::OnAuthFailure(const AuthFailure& error) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::RecoverEncryptedData(
    std::unique_ptr<UserContext> user_context,
    const std::string& old_password) {
  LOGIN_LOG(USER) << "Attempting to update password";
  VLOG(1) << "AuthSessionAuthenticator::RecoverEncryptedData";

  const cryptohome::KeyDefinition* password_key_def =
      user_context->GetAuthFactorsData().FindOnlinePasswordKey();
  DCHECK(password_key_def);
  const std::string key_label = password_key_def->label;

  if (!user_context->HasReplacementKey()) {
    // Assume that there was an attempt to use the key, so it is was already
    // hashed.
    DCHECK(user_context->GetKey()->GetKeyType() !=
           Key::KEY_TYPE_PASSWORD_PLAIN);
    // Make sure that the key has correct label.
    user_context->GetKey()->SetLabel(key_label);
    user_context->SaveKeyForReplacement();
  }

  chromeos::Key auth_key(old_password);
  auth_key.SetLabel(key_label);
  user_context->SetKey(auth_key);

  DCHECK(!user_context->GetAuthSessionId().empty());

  // (1) Transform old password key (as a password)
  // (2) Authenticate AuthSession
  // (3) Replace key with the one that was validated by online sign-in
  // (4) Mount directory
  // (5) (Safe mode) Check ownership
  // (#) Notify success
  // (*) Errors are reported as COULD_NOT_MOUNT_CRYPTOHOME

  // Callbacks are created in reverse order:

  // (*)
  auto error_handler_repeating = base::BindRepeating(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(),
      /*default_reason=*/AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  // (#)
  auto success_notification = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());
  // (5)
  ContextCallback safe_mode_check = base::BindOnce(
      &AuthSessionAuthenticator::RunSafeModeChecks, weak_factory_.GetWeakPtr(),
      /*continuation=*/std::move(success_notification));
  // (4.1)
  ConfigureMountCallback mount_regular_cfg =
      base::BindOnce(&ConfigureGenericMount);
  // (4)
  ContextCallback mount_existing = base::BindOnce(
      &AuthSessionAuthenticator::MountGeneric, weak_factory_.GetWeakPtr(),
      /*error_handler=*/base::BindOnce(error_handler_repeating),
      /*configurator=*/std::move(mount_regular_cfg),
      /*continuation=*/std::move(safe_mode_check));
  // (3)
  ContextCallback replace_key =
      base::BindOnce(&AuthSessionAuthenticator::UpdateCredentialsGeneric,
                     weak_factory_.GetWeakPtr(),
                     /*error_handler=*/base::BindOnce(error_handler_repeating),
                     /*continuation=*/std::move(mount_existing));

  // (2.1)
  ErrorHandlingCallback auth_error_handler =
      base::BindOnce(&AuthSessionAuthenticator::
                         ExistingUserPasswordAuthenticationErrorHandling,
                     weak_factory_.GetWeakPtr(),
                     /*fallback=*/base::BindOnce(error_handler_repeating),
                     /*verified_password=*/true);
  // (2)
  ExistingUserAuthSessionCallback existing_user_flow = base::BindOnce(
      &AuthSessionAuthenticator::AuthenticateSessionGeneric,
      weak_factory_.GetWeakPtr(),
      /*error_handler=*/std::move(auth_error_handler),
      /*key_transformer=*/base::BindOnce(&TransformToWildcardKey),
      /*continuation=*/std::move(replace_key));

  // (1)
  HashPassword(std::move(user_context), std::move(existing_user_flow));
}

void AuthSessionAuthenticator::ResyncEncryptedData(
    std::unique_ptr<UserContext> user_context) {
  LOGIN_LOG(USER) << "Attempting to re-create cryptohome";
  VLOG(1) << "AuthSessionAuthenticator::ResyncEncryptedData";
  auto error_handler =
      base::BindOnce(&AuthSessionAuthenticator::ProcessCryptohomeError,
                     weak_factory_.GetWeakPtr(),
                     /*default_reason=*/AuthFailure::DATA_REMOVAL_FAILED);

  ContextCallback continuation = base::BindOnce(
      &AuthSessionAuthenticator::CompleteLoginImpl, weak_factory_.GetWeakPtr());

  RemoveGeneric(std::move(error_handler), std::move(continuation),
                std::move(user_context));
}

void AuthSessionAuthenticator::PrepareForNewAttempt(
    const std::string& method_id,
    const std::string& long_desc) {
  LOGIN_LOG(USER) << "Authentication attempt : " << long_desc;
  VLOG(1) << "AuthSessionAuthenticator::" << method_id;

  // Assume no ongoing authentication requests happen at the moment.
  DCHECK(!weak_factory_.HasWeakPtrs());
  // Clear all ongoing requests
  weak_factory_.InvalidateWeakPtrs();
}

void AuthSessionAuthenticator::ProcessCryptohomeError(
    AuthFailure::FailureReason default_error,
    std::unique_ptr<UserContext> context,
    user_data_auth::CryptohomeErrorCode error) {
  if (!consumer_)
    return;

  switch (error) {
    case user_data_auth::CRYPTOHOME_ERROR_NOT_SET:
      NOTREACHED() << "Should be called with an error";
      return;
    case user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND:
      NotifyFailure(AuthFailure::MISSING_CRYPTOHOME, std::move(context));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_NOT_IMPLEMENTED:
    case user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT:
    case user_data_auth::CRYPTOHOME_TOKEN_SERIALIZATION_FAILED:
      // Fatal implementation errors
      break;
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_ERROR_INTERNAL:
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED:
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_DENIED:
      // Fingerprint errors
      break;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_FATAL:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_QUOTA_EXCEEDED:
    case user_data_auth::CRYPTOHOME_ERROR_BACKING_STORE_FAILURE:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_FINALIZE_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_GET_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_SET_FAILED:
      // Fatal system state errors
      break;
    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND:
    case user_data_auth::CRYPTOHOME_ERROR_MIGRATE_KEY_FAILED:
    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED:

    case user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED:
    case user_data_auth::CRYPTOHOME_ERROR_KEY_LABEL_EXISTS:
    case user_data_auth::CRYPTOHOME_ERROR_UPDATE_SIGNATURE_INVALID:
      // Assumptions about key are not correct
      break;
    case user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN:
      // Auth session expired, might need to handle it separately later.
      break;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_COMM_ERROR:
    case user_data_auth::CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT:
      NotifyFailure(AuthFailure::TPM_ERROR, std::move(context));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK:
      NotifyFailure(AuthFailure::TPM_ERROR, std::move(context));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY:
      // Assumption about system state is not correct
      break;
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION:
    case user_data_auth::CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE:
      NOTREACHED() << "Encryption migration should be handled separately";
      return;
    case user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED:
      NOTREACHED() << "Add credentials failure should be handled separately";
      return;
    case user_data_auth::CRYPTOHOME_ERROR_REMOVE_FAILED:
      NotifyFailure(AuthFailure::DATA_REMOVAL_FAILED, std::move(context));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_TPM_UPDATE_REQUIRED:
      NotifyFailure(AuthFailure::TPM_UPDATE_REQUIRED, std::move(context));
      return;
    case user_data_auth::CRYPTOHOME_ERROR_VAULT_UNRECOVERABLE:
      NotifyFailure(AuthFailure::UNRECOVERABLE_CRYPTOHOME, std::move(context));
      return;
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
      // Also fatal errors that should not be surfaced.
      NOTREACHED();
      return;
    case user_data_auth::CryptohomeErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case user_data_auth::CryptohomeErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_:
      // Ignored
      return;
    default:
      // We need the default case here so that it is possible to add new
      // CryptohomeErrorCode, because CryptohomeErrorCode is defined in another
      // repo.
      // However, we should seek to handle all CryptohomeErrorCode and not let
      // any of them hit the default block.
      NOTREACHED() << "Unhandled CryptohomeErrorCode in ProcessCryptohomeError"
                      ": "
                   << static_cast<int>(error);
  }
  NotifyFailure(default_error, std::move(context));
}

void AuthSessionAuthenticator::CreateAuthSessionGeneric(
    const std::string& user_type,
    ErrorHandlingCallback error_handler,
    ConfigureAuthSessionCallback configurator,
    KeyHashingCallback key_hasher,
    NewUserAuthSessionCallback new_user_flow,
    ExistingUserAuthSessionCallback existing_user_flow,
    std::unique_ptr<UserContext> context) {
  user_data_auth::StartAuthSessionRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());

  request = std::move(configurator).Run(*context, std::move(request));

  UserDataAuthClient::StartAuthSessionCallback auth_session_callback =
      base::BindOnce(&AuthSessionAuthenticator::OnAuthSessionCreatedGeneric,
                     weak_factory_.GetWeakPtr(), user_type,
                     std::move(error_handler), std::move(key_hasher),
                     std::move(new_user_flow), std::move(existing_user_flow),
                     std::move(context));

  UserDataAuthClient::Get()->StartAuthSession(request,
                                              std::move(auth_session_callback));
}

void AuthSessionAuthenticator::OnAuthSessionCreatedGeneric(
    const std::string& user_type,
    ErrorHandlingCallback error_handler,
    KeyHashingCallback key_hasher,
    NewUserAuthSessionCallback new_user_flow,
    ExistingUserAuthSessionCallback existing_user_flow,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::StartAuthSessionReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::AuthSessionCreated " << user_type;

  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "StartAuthSession for " << user_type
                     << " failed with error " << error;
    std::move(error_handler).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  context->SetAuthSessionId(reply->auth_session_id());

  std::vector<cryptohome::KeyDefinition> key_definitions;
  for (const auto& [label, key_data] : reply->key_label_data()) {
    key_definitions.push_back(KeyDataToKeyDefinition(key_data));
  }

  AuthFactorsData auth_factors_data(std::move(key_definitions));
  context->SetAuthFactorsData(std::move(auth_factors_data));

  ContextCallback consumer;
  if (reply->user_exists()) {
    consumer = std::move(existing_user_flow);
  } else {
    LOGIN_LOG(EVENT) << "User is new";
    consumer = std::move(new_user_flow);
  }
  std::move(key_hasher).Run(std::move(context), std::move(consumer));
}

void AuthSessionAuthenticator::AuthenticateSessionGeneric(
    ErrorHandlingCallback auth_error_callback,
    TransformCryotohomeKeyCallback transformer,
    ContextCallback continuation,
    std::unique_ptr<UserContext> transformed_context) {
  user_data_auth::AuthenticateAuthSessionRequest request;
  request.set_auth_session_id(transformed_context->GetAuthSessionId());
  std::move(transformer)
      .Run(*transformed_context,
           request.mutable_authorization()->mutable_key());
  auto authentication_callback =
      base::BindOnce(&AuthSessionAuthenticator::OnAuthenticateSessionGeneric,
                     weak_factory_.GetWeakPtr(), std::move(auth_error_callback),
                     std::move(continuation), std::move(transformed_context));

  UserDataAuthClient::Get()->AuthenticateAuthSession(
      request, std::move(authentication_callback));
}

void AuthSessionAuthenticator::OnAuthenticateSessionGeneric(
    ErrorHandlingCallback error_handler,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::AuthenticateAuthSessionReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnAuthenticateSession";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "AuthenticateSession failed with error " << error;
    std::move(error_handler).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());

  // No 2FA support yet.
  DCHECK(reply->authenticated());

  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::AddInitialCredentialsGeneric(
    ErrorHandlingCallback error_handler,
    TransformCryotohomeKeyCallback transformer,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  user_data_auth::AddCredentialsRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_add_more_credentials(false);

  std::move(transformer)
      .Run(*context, request.mutable_authorization()->mutable_key());

  UserDataAuthClient::AddCredentialsCallback add_credentials_callback =
      base::BindOnce(&AuthSessionAuthenticator::OnAddInitialCredentialsGeneric,
                     weak_factory_.GetWeakPtr(), std::move(error_handler),
                     std::move(continuation), std::move(context));

  UserDataAuthClient::Get()->AddCredentials(
      request, std::move(add_credentials_callback));
}

void AuthSessionAuthenticator::OnAddInitialCredentialsGeneric(
    ErrorHandlingCallback error_handler,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::AddCredentialsReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnAddInitialCredentials";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "AddCredentials(initial) failed with error " << error;
    if (error == user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED) {
      // TODO(antrim): implement correct handling
      NOTIMPLEMENTED();
      return;
    }
    std::move(error_handler).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::MountGeneric(
    ErrorHandlingCallback error_handler,
    ConfigureMountCallback configurator,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  VLOG(1) << "AuthSessionAuthenticator::Mount";
  user_data_auth::MountRequest mount;

  mount.set_auth_session_id(context->GetAuthSessionId());

  if (configurator)
    mount = std::move(configurator).Run(*context, std::move(mount));

  ErrorHandlingCallback mount_error_handler =
      base::BindOnce(&AuthSessionAuthenticator::MountErrorHandling,
                     weak_factory_.GetWeakPtr(), std::move(error_handler));

  UserDataAuthClient::Get()->Mount(
      mount,
      base::BindOnce(&AuthSessionAuthenticator::OnMountGeneric,
                     weak_factory_.GetWeakPtr(), std::move(mount_error_handler),
                     std::move(continuation), std::move(context)));
}

void AuthSessionAuthenticator::OnMountGeneric(
    ErrorHandlingCallback error_callback,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::MountReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnMount";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Mount failed with error " << error;
    std::move(error_callback).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  context->SetUserIDHash(reply->sanitized_username());

  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::UnmountGeneric(
    ErrorHandlingCallback error_handler,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  VLOG(1) << "AuthSessionAuthenticator::UnMount";

  user_data_auth::UnmountRequest request;
  UserDataAuthClient::Get()->Unmount(
      request,
      base::BindOnce(&AuthSessionAuthenticator::OnUnmountGeneric,
                     weak_factory_.GetWeakPtr(), std::move(error_handler),
                     std::move(continuation), std::move(context)));
}

void AuthSessionAuthenticator::OnUnmountGeneric(
    ErrorHandlingCallback error_callback,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::UnmountReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnUnmount";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Unmount failed with error" << error;
    std::move(error_callback).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::RemoveGeneric(
    ErrorHandlingCallback error_handler,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  VLOG(1) << "AuthSessionAuthenticator::Remove";

  user_data_auth::RemoveRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  UserDataAuthClient::Get()->Remove(
      request,
      base::BindOnce(&AuthSessionAuthenticator::OnRemoveGeneric,
                     weak_factory_.GetWeakPtr(), std::move(error_handler),
                     std::move(continuation), std::move(context)));
}

void AuthSessionAuthenticator::OnRemoveGeneric(
    ErrorHandlingCallback error_callback,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::RemoveReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnRemove";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Removal failed with error " << error;
    std::move(error_callback).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  // Removing user directory invalidates session.
  context->ResetAuthSessionId();
  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::UpdateCredentialsGeneric(
    ErrorHandlingCallback error_handler,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  VLOG(1) << "AuthSessionAuthenticator::UpdateCredentials";

  user_data_auth::UpdateCredentialRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_old_credential_label(context->GetKey()->GetLabel());
  DCHECK(context->HasReplacementKey());
  const Key* key = context->GetReplacementKey();
  CHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());
  cryptohome::KeyDefinitionToKey(
      cryptohome::KeyDefinition::CreateForPassword(
          key->GetSecret(), key->GetLabel(), cryptohome::PRIV_DEFAULT),
      request.mutable_authorization()->mutable_key());

  UserDataAuthClient::Get()->UpdateCredential(
      request,
      base::BindOnce(&AuthSessionAuthenticator::OnUpdateCredentialsGeneric,
                     weak_factory_.GetWeakPtr(), std::move(error_handler),
                     std::move(continuation), std::move(context)));
}

void AuthSessionAuthenticator::OnUpdateCredentialsGeneric(
    ErrorHandlingCallback error_callback,
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::UpdateCredentialReply> reply) {
  VLOG(1) << "AuthSessionAuthenticator::OnUpdateCredentials";
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (error != user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "Update failed with error " << error;
    std::move(error_callback).Run(std::move(context), error);
    return;
  }
  CHECK(reply.has_value());
  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::ExistingUserPasswordAuthenticationErrorHandling(
    ErrorHandlingCallback fallback,
    bool verified_password,
    std::unique_ptr<UserContext> context,
    user_data_auth::CryptohomeErrorCode error) {
  if ((error == user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED) &&
      verified_password) {
    LOGIN_LOG(EVENT) << "Password change detected";
    if (!consumer_)
      return;
    consumer_->OnPasswordChangeDetected(*context);
    return;
  }
  std::move(fallback).Run(std::move(context), error);
}

void AuthSessionAuthenticator::NonOwnerUnmountErrorHandler(
    std::unique_ptr<UserContext> context,
    user_data_auth::CryptohomeErrorCode error) {
  // Crash if could not unmount home directory, and let session_manager
  // handle it.
  LOG(FATAL) << "Failed to unmount non-owner home directory " << error;
}

void AuthSessionAuthenticator::MountErrorHandling(
    ErrorHandlingCallback fallback,
    std::unique_ptr<UserContext> context,
    user_data_auth::CryptohomeErrorCode error) {
  if (error == user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION ||
      error == user_data_auth::
                   CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE) {
    LOGIN_LOG(EVENT) << "Old encryption detected";
    if (!consumer_)
      return;
    consumer_->OnOldEncryptionDetected(
        *context, error ==
                      user_data_auth::
                          CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE);
    return;
  }
  std::move(fallback).Run(std::move(context), error);
}

void AuthSessionAuthenticator::NotifyAuthSuccess(
    std::unique_ptr<UserContext> context) {
  if (consumer_)
    consumer_->OnAuthSuccess(*context);
}

void AuthSessionAuthenticator::NotifyGuestSuccess(
    std::unique_ptr<UserContext> context) {
  if (consumer_)
    consumer_->OnOffTheRecordAuthSuccess();
}

void AuthSessionAuthenticator::NotifyFailure(
    AuthFailure::FailureReason reason,
    std::unique_ptr<UserContext> context) {
  if (consumer_)
    consumer_->OnAuthFailure(AuthFailure(reason));
}

void AuthSessionAuthenticator::FailIfInSafeMode(
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Safe mode: non-owner";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }
  std::move(continuation).Run(std::move(context));
}

void AuthSessionAuthenticator::RunSafeModeChecks(
    ContextCallback continuation,
    std::unique_ptr<UserContext> context) {
  if (!safe_mode_delegate_->IsSafeMode()) {
    std::move(continuation).Run(std::move(context));
    return;
  }
  LOGIN_LOG(EVENT) << "Running in safe mode";
  // Save value as context will be moved.
  auto user_hash = context->GetUserIDHash();
  // Device is running in the safe mode, need to check if user is an owner.
  safe_mode_delegate_->CheckSafeModeOwnership(
      user_hash,
      base::BindOnce(&AuthSessionAuthenticator::OnOwnershipCheckedForSafeMode,
                     weak_factory_.GetWeakPtr(), std::move(continuation),
                     std::move(context)));
}

void AuthSessionAuthenticator::OnOwnershipCheckedForSafeMode(
    ContextCallback continuation,
    std::unique_ptr<UserContext> context,
    bool is_owner) {
  if (is_owner) {
    LOGIN_LOG(EVENT) << "Safe mode: owner";
    std::move(continuation).Run(std::move(context));
    return;
  }
  LOGIN_LOG(EVENT) << "Safe mode: non-owner";
  // Unmount home directory
  //   (1) Notify that owner is required upon success
  //   (2) Crash if directory could not be unmounted

  // (1)
  ContextCallback not_owner =
      base::BindOnce(&AuthSessionAuthenticator::NotifyFailure,
                     weak_factory_.GetWeakPtr(), AuthFailure::OWNER_REQUIRED);
  // (2)
  ErrorHandlingCallback crasher =
      base::BindOnce(&AuthSessionAuthenticator::NonOwnerUnmountErrorHandler,
                     weak_factory_.GetWeakPtr());
  UnmountGeneric(std::move(crasher), std::move(not_owner), std::move(context));
}

}  // namespace ash
