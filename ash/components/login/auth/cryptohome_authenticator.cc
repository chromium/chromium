// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/cryptohome_authenticator.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/components/login/auth/auth_status_consumer.h"
#include "ash/components/login/auth/cryptohome_parameter_utils.h"
#include "ash/components/login/auth/public/auth_failure.h"
#include "ash/components/login/auth/public/cryptohome_key_constants.h"
#include "ash/components/login/auth/public/key.h"
#include "ash/components/login/auth/public/user_context.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/metrics/login_event_recorder.h"
#include "components/account_id/account_id.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using cryptohome::KeyLabel;

namespace ash {

namespace {

bool ShouldUseOldEncryptionForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kCryptohomeUseOldEncryptionForTesting);
}

// The name under which the type of key generated from the user's GAIA
// credentials is stored.
const char kKeyProviderDataTypeName[] = "type";

// The name under which the salt used to generate a key from the user's GAIA
// credentials is stored.
const char kKeyProviderDataSaltName[] = "salt";

// Returns a human-readable string describing |state|.
const char* AuthStateToString(CryptohomeAuthenticator::AuthState state) {
  switch (state) {
    case CryptohomeAuthenticator::CONTINUE:
      return "CONTINUE";
    case CryptohomeAuthenticator::NO_MOUNT:
      return "NO_MOUNT";
    case CryptohomeAuthenticator::FAILED_MOUNT:
      return "FAILED_MOUNT";
    case CryptohomeAuthenticator::FAILED_REMOVE:
      return "FAILED_REMOVE";
    case CryptohomeAuthenticator::FAILED_TMPFS:
      return "FAILED_TMPFS";
    case CryptohomeAuthenticator::FAILED_TPM:
      return "FAILED_TPM";
    case CryptohomeAuthenticator::CREATE_NEW:
      return "CREATE_NEW";
    case CryptohomeAuthenticator::RECOVER_MOUNT:
      return "RECOVER_MOUNT";
    case CryptohomeAuthenticator::POSSIBLE_PW_CHANGE:
      return "POSSIBLE_PW_CHANGE";
    case CryptohomeAuthenticator::NEED_NEW_PW:
      return "NEED_NEW_PW";
    case CryptohomeAuthenticator::NEED_OLD_PW:
      return "NEED_OLD_PW";
    case CryptohomeAuthenticator::HAVE_NEW_PW:
      return "HAVE_NEW_PW";
    case CryptohomeAuthenticator::OFFLINE_LOGIN:
      return "OFFLINE_LOGIN";
    case CryptohomeAuthenticator::ONLINE_LOGIN:
      return "ONLINE_LOGIN";
    case CryptohomeAuthenticator::UNLOCK:
      return "UNLOCK";
    case CryptohomeAuthenticator::ONLINE_FAILED:
      return "ONLINE_FAILED";
    case CryptohomeAuthenticator::GUEST_LOGIN:
      return "GUEST_LOGIN";
    case CryptohomeAuthenticator::PUBLIC_ACCOUNT_LOGIN:
      return "PUBLIC_ACCOUNT_LOGIN";
    case CryptohomeAuthenticator::LOGIN_FAILED:
      return "LOGIN_FAILED";
    case CryptohomeAuthenticator::OWNER_REQUIRED:
      return "OWNER_REQUIRED";
    case CryptohomeAuthenticator::FAILED_USERNAME_HASH:
      return "FAILED_USERNAME_HASH";
    case CryptohomeAuthenticator::KIOSK_ACCOUNT_LOGIN:
      return "KIOSK_ACCOUNT_LOGIN";
    case CryptohomeAuthenticator::REMOVED_DATA_AFTER_FAILURE:
      return "REMOVED_DATA_AFTER_FAILURE";
    case CryptohomeAuthenticator::FAILED_OLD_ENCRYPTION:
      return "FAILED_OLD_ENCRYPTION";
    case CryptohomeAuthenticator::FAILED_PREVIOUS_MIGRATION_INCOMPLETE:
      return "FAILED_PREVIOUS_MIGRATION_INCOMPLETE";
    case CryptohomeAuthenticator::OFFLINE_NO_MOUNT:
      return "OFFLINE_NO_MOUNT";
    case CryptohomeAuthenticator::TPM_UPDATE_REQUIRED:
      return "TPM_UPDATE_REQUIRED";
    case CryptohomeAuthenticator::OFFLINE_MOUNT_UNRECOVERABLE:
      return "OFFLINE_MOUNT_UNRECOVERABLE";
  }
  return "UNKNOWN";
}

// Hashes |key| with |system_salt| if it its type is KEY_TYPE_PASSWORD_PLAIN.
// Returns the keys unmodified otherwise.
std::unique_ptr<Key> TransformKeyIfNeeded(const Key& key,
                                          const std::string& system_salt) {
  std::unique_ptr<Key> result(new Key(key));
  if (result->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN)
    result->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  return result;
}

// Records get hash status and calls resolver->Resolve().
void TriggerResolveHash(const base::WeakPtr<AuthAttemptState>& attempt,
                        scoped_refptr<CryptohomeAuthenticator> resolver,
                        bool success,
                        const std::string& username_hash) {
  if (success)
    attempt->RecordUsernameHash(username_hash);
  else
    attempt->RecordUsernameHashFailed();
  resolver->Resolve();
}

// Records an error in accessing the user's cryptohome with the given key and
// calls resolver->Resolve() after adding a login time marker.
void RecordKeyErrorAndResolve(const base::WeakPtr<AuthAttemptState>& attempt,
                              scoped_refptr<CryptohomeAuthenticator> resolver) {
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker("CryptohomeMount-End",
                                                          false);
  attempt->RecordCryptohomeStatus(cryptohome::MOUNT_ERROR_KEY_FAILURE);
  resolver->Resolve();
}

// Callback invoked when cryptohome's GetSanitizedUsername() method has
// finished.
void OnGetSanitizedUsername(
    base::OnceCallback<void(bool, const std::string&)> callback,
    absl::optional<user_data_auth::GetSanitizedUsernameReply> reply) {
  std::string res;
  if (reply.has_value())
    res = reply->sanitized_username();
  std::move(callback).Run(!res.empty(), res);
}

// Callback invoked when a cryptohome *Ex method, which only returns a
// base::Reply, finishes.
template <typename ReplyType>
void OnReplyMethod(const base::WeakPtr<AuthAttemptState>& attempt,
                   scoped_refptr<CryptohomeAuthenticator> resolver,
                   const char* time_marker,
                   absl::optional<ReplyType> reply) {
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(time_marker, false);
  attempt->RecordCryptohomeStatus(user_data_auth::ReplyToMountError(reply));
  resolver->Resolve();
}

// Callback invoked when cryptohome's MountEx() method has finished.
void OnMount(const base::WeakPtr<AuthAttemptState>& attempt,
             scoped_refptr<CryptohomeAuthenticator> resolver,
             absl::optional<user_data_auth::MountReply> reply) {
  const bool public_mount = attempt->user_context->GetUserType() ==
                                user_manager::USER_TYPE_KIOSK_APP ||
                            attempt->user_context->GetUserType() ==
                                user_manager::USER_TYPE_ARC_KIOSK_APP ||
                            attempt->user_context->GetUserType() ==
                                user_manager::USER_TYPE_WEB_KIOSK_APP;

  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(
      public_mount ? "CryptohomeMountPublic-End" : "CryptohomeMount-End",
      false);
  attempt->RecordCryptohomeStatus(user_data_auth::ReplyToMountError(reply));
  if (attempt->cryptohome_code() == cryptohome::MOUNT_ERROR_NONE) {
    attempt->RecordUsernameHash(reply->sanitized_username());
  } else {
    LOGIN_LOG(ERROR) << "MountEx failed. Error: " << attempt->cryptohome_code();
    attempt->RecordUsernameHashFailed();
  }
  resolver->Resolve();
}

// Calls cryptohome's MountEx() method. The key in |attempt->user_context| must
// not be a plain text password. If the user provided a plain text password,
// that password must be transformed to another key type (by salted hashing)
// before calling this method.
void DoMount(const base::WeakPtr<AuthAttemptState>& attempt,
             scoped_refptr<CryptohomeAuthenticator> resolver,
             bool ephemeral,
             bool create_if_nonexistent) {
  const Key* key = attempt->user_context->GetKey();
  // If the |key| is a plain text password, crash rather than attempting to
  // mount the cryptohome with a plain text password.
  CHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());

  // Set state that username_hash is requested here so that test implementation
  // that returns directly would not generate 2 OnLoginSuccess() calls.
  attempt->UsernameHashRequested();

  const cryptohome::AuthorizationRequest auth =
      CreateAuthorizationRequestFromKeyDef(
          cryptohome_parameter_utils::CreateAuthorizationKeyDefFromUserContext(
              *attempt->user_context));

  user_data_auth::MountRequest mount;
  if (ephemeral)
    mount.set_require_ephemeral(true);
  if (create_if_nonexistent) {
    cryptohome::KeyDefinitionToKey(
        cryptohome_parameter_utils::CreateKeyDefFromUserContext(
            *attempt->user_context),
        mount.mutable_create()->add_keys());
    if (ShouldUseOldEncryptionForTesting()) {
      mount.mutable_create()->set_force_ecryptfs(true);
    }
  }
  if (attempt->user_context->IsForcingDircrypto() &&
      !ShouldUseOldEncryptionForTesting()) {
    mount.set_force_dircrypto_if_available(true);
  }
  *mount.mutable_account() = cryptohome::CreateAccountIdentifierFromAccountId(
      attempt->user_context->GetAccountId());
  *mount.mutable_authorization() = auth;
  UserDataAuthClient::Get()->Mount(mount,
                                   base::BindOnce(&OnMount, attempt, resolver));
}

// Callback invoked when the system salt has been retrieved. Transforms the key
// in |attempt->user_context| using Chrome's default hashing algorithm and the
// system salt, then calls MountEx().
void OnGetSystemSalt(const base::WeakPtr<AuthAttemptState>& attempt,
                     scoped_refptr<CryptohomeAuthenticator> resolver,
                     bool ephemeral,
                     bool create_if_nonexistent,
                     const std::string& system_salt) {
  DCHECK_EQ(Key::KEY_TYPE_PASSWORD_PLAIN,
            attempt->user_context->GetKey()->GetKeyType());

  attempt->user_context->GetKey()->Transform(
      Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  DoMount(attempt, resolver, ephemeral, create_if_nonexistent);
}

// Callback invoked when cryptohome's GetKeyData() method has finished.
// * If GetKeyData() returned metadata indicating the hashing algorithm and
//   salt that were used to generate the key for this user's cryptohome,
//   transforms the key in |attempt->user_context| with the same parameters.
// * Otherwise, starts the retrieval of the system salt so that the key in
//   |attempt->user_context| can be transformed with Chrome's default hashing
//   algorithm and the system salt.
// The resulting key is then passed to cryptohome's MountEx().
void OnGetKeyData(const base::WeakPtr<AuthAttemptState>& attempt,
                  scoped_refptr<CryptohomeAuthenticator> resolver,
                  bool ephemeral,
                  bool create_if_nonexistent,
                  absl::optional<user_data_auth::GetKeyDataReply> reply) {
  if (user_data_auth::ReplyToMountError(reply) ==
      cryptohome::MOUNT_ERROR_NONE) {
    std::vector<cryptohome::KeyDefinition> key_definitions =
        user_data_auth::GetKeyDataReplyToKeyDefinitions(reply);
    if (key_definitions.size() == 1) {
      const cryptohome::KeyDefinition& key_definition = key_definitions.front();
      DCHECK_EQ(kCryptohomeGaiaKeyLabel, key_definition.label.value());

      // Extract the key type and salt from |key_definition|, if present.
      std::unique_ptr<int64_t> type;
      std::unique_ptr<std::string> salt;
      for (std::vector<cryptohome::KeyDefinition::ProviderData>::const_iterator
               it = key_definition.provider_data.begin();
           it != key_definition.provider_data.end(); ++it) {
        if (it->name == kKeyProviderDataTypeName) {
          if (it->number)
            type = std::make_unique<int64_t>(*it->number);
          else
            NOTREACHED();
        } else if (it->name == kKeyProviderDataSaltName) {
          if (it->bytes)
            salt = std::make_unique<std::string>(*it->bytes);
          else
            NOTREACHED();
        }
      }

      if (type) {
        if (*type < 0 || *type >= Key::KEY_TYPE_COUNT) {
          LOGIN_LOG(ERROR) << "Invalid key type: " << *type;
          RecordKeyErrorAndResolve(attempt, resolver);
          return;
        }

        if (!salt) {
          LOGIN_LOG(ERROR) << "Missing salt.";
          RecordKeyErrorAndResolve(attempt, resolver);
          return;
        }

        attempt->user_context->GetKey()->Transform(
            static_cast<Key::KeyType>(*type), *salt);
        DoMount(attempt, resolver, ephemeral, create_if_nonexistent);
        return;
      }
    } else {
      LOGIN_LOG(EVENT) << "GetKeyDataEx() returned " << key_definitions.size()
                       << " entries.";
    }
  }

  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &OnGetSystemSalt, attempt, resolver, ephemeral, create_if_nonexistent));
}

// Starts the process that will mount a user's cryptohome.
// * If the key in |attempt->user_context| is not a plain text password,
//   cryptohome's MountEx() method is called directly with the key.
// * Otherwise, the key must be transformed (by salted hashing) before being
//   passed to MountEx(). In that case, cryptohome's GetKeyDataEx() method is
//   called to retrieve metadata indicating the hashing algorithm and salt that
//   were used to generate the key for this user's cryptohome and the key is
//   transformed accordingly before calling MountEx().
void StartMount(const base::WeakPtr<AuthAttemptState>& attempt,
                scoped_refptr<CryptohomeAuthenticator> resolver,
                bool ephemeral,
                bool create_if_nonexistent) {
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(
      "CryptohomeMount-Start", false);

  if (attempt->user_context->GetKey()->GetKeyType() !=
      Key::KEY_TYPE_PASSWORD_PLAIN) {
    DoMount(attempt, resolver, ephemeral, create_if_nonexistent);
    return;
  }

  user_data_auth::GetKeyDataRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(
          attempt->user_context->GetAccountId());
  // Calling mutable_authorization_request() to ensure
  // has_authorization_request() would return true.
  request.mutable_authorization_request();
  request.mutable_key()->mutable_data()->set_label(kCryptohomeGaiaKeyLabel);
  UserDataAuthClient::Get()->GetKeyData(
      request, base::BindOnce(&OnGetKeyData, attempt, resolver, ephemeral,
                              create_if_nonexistent));
}

// Calls cryptohome's mount method for guest and also get the user hash from
// cryptohome.
void MountGuestAndGetHash(const base::WeakPtr<AuthAttemptState>& attempt,
                          scoped_refptr<CryptohomeAuthenticator> resolver) {
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(
      "CryptohomeMountGuest-Start", false);
  attempt->UsernameHashRequested();

  user_data_auth::MountRequest guest_mount_request;
  guest_mount_request.set_guest_mount(true);
  UserDataAuthClient::Get()->Mount(
      guest_mount_request,
      base::BindOnce(&OnReplyMethod<user_data_auth::MountReply>, attempt,
                     resolver, "CryptohomeMountGuest-End"));

  user_data_auth::GetSanitizedUsernameRequest sanitized_username_request;
  sanitized_username_request.set_username(
      cryptohome::CreateAccountIdentifierFromAccountId(
          attempt->user_context->GetAccountId())
          .account_id());
  CryptohomeMiscClient::Get()->GetSanitizedUsername(
      sanitized_username_request,
      base::BindOnce(&OnGetSanitizedUsername,
                     base::BindOnce(&TriggerResolveHash, attempt, resolver)));
}

// Calls cryptohome's MountEx method with the public_mount option.
void MountPublic(const base::WeakPtr<AuthAttemptState>& attempt,
                 scoped_refptr<CryptohomeAuthenticator> resolver,
                 bool force_dircrypto_if_available) {
  user_data_auth::MountRequest mount;
  if (force_dircrypto_if_available)
    mount.set_force_dircrypto_if_available(true);
  mount.set_public_mount(true);
  // Set the request to create a new homedir when missing.
  cryptohome::KeyDefinitionToKey(
      cryptohome::KeyDefinition::CreateForPassword(
          std::string(), KeyLabel(kCryptohomePublicMountLabel),
          cryptohome::PRIV_DEFAULT),
      mount.mutable_create()->add_keys());

  // For public mounts, authorization secret is filled by cryptohomed, hence it
  // is left empty. Authentication's key label is also set to an empty string,
  // which is a wildcard allowing any key to match to allow cryptohomes created
  // in a legacy way. (See comments in DoMount.)
  *mount.mutable_account() = cryptohome::CreateAccountIdentifierFromAccountId(
      attempt->user_context->GetAccountId());
  // Calling mutable_authorization() to ensure has_authorization() would return
  // true.
  mount.mutable_authorization();
  UserDataAuthClient::Get()->Mount(mount,
                                   base::BindOnce(&OnMount, attempt, resolver));
}

// Calls cryptohome's key migration method.
void Migrate(const base::WeakPtr<AuthAttemptState>& attempt,
             scoped_refptr<CryptohomeAuthenticator> resolver,
             bool passing_old_hash,
             const std::string& old_password,
             const std::string& system_salt) {
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(
      "CryptohomeMigrate-Start", false);

  cryptohome::AccountIdentifier account_id =
      cryptohome::CreateAccountIdentifierFromAccountId(
          attempt->user_context->GetAccountId());

  cryptohome::AuthorizationRequest auth_request;
  user_data_auth::MigrateKeyRequest migrate_request;
  // TODO(bartfab): Retrieve the hashing algorithm and salt to use for |old_key|
  // from cryptohomed.
  std::unique_ptr<Key> old_key =
      TransformKeyIfNeeded(Key(old_password), system_salt);
  std::unique_ptr<Key> new_key =
      TransformKeyIfNeeded(*attempt->user_context->GetKey(), system_salt);
  if (passing_old_hash) {
    auth_request.mutable_key()->set_secret(old_key->GetSecret());
    migrate_request.set_secret(new_key->GetSecret());
  } else {
    auth_request.mutable_key()->set_secret(new_key->GetSecret());
    migrate_request.set_secret(old_key->GetSecret());
  }

  *migrate_request.mutable_account_id() = account_id;
  *migrate_request.mutable_authorization_request() = auth_request;
  UserDataAuthClient::Get()->MigrateKey(
      migrate_request,
      base::BindOnce(&OnReplyMethod<user_data_auth::MigrateKeyReply>, attempt,
                     resolver, "CryptohomeMigrate-End"));
}

// Calls cryptohome's remove method.
void Remove(const base::WeakPtr<AuthAttemptState>& attempt,
            scoped_refptr<CryptohomeAuthenticator> resolver) {
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(
      "CryptohomeRemove-Start", false);

  cryptohome::AccountIdentifier account_id;
  account_id.set_account_id(
      cryptohome::Identification(attempt->user_context->GetAccountId()).id());

  user_data_auth::RemoveRequest req;
  *req.mutable_identifier() = account_id;
  UserDataAuthClient::Get()->Remove(
      req, base::BindOnce(&OnReplyMethod<user_data_auth::RemoveReply>, attempt,
                          resolver, "CryptohomeRemove-End"));
}

}  // namespace

CryptohomeAuthenticator::CryptohomeAuthenticator(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    PrefService* local_state,
    std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
    AuthStatusConsumer* consumer)
    : Authenticator(consumer),
      task_runner_(std::move(task_runner)),
      local_state_(local_state),
      safe_mode_delegate_(std::move(safe_mode_delegate)),
      delayed_login_failure_(AuthFailure::NONE) {}

void CryptohomeAuthenticator::AuthenticateToLogin(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  DCHECK(user_context->GetUserType() == user_manager::USER_TYPE_REGULAR ||
         user_context->GetUserType() == user_manager::USER_TYPE_CHILD ||
         user_context->GetUserType() ==
             user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  current_state_ = std::make_unique<AuthAttemptState>(std::move(user_context));
  // Reset the verified flag.
  owner_is_verified_ = false;

  StartMount(current_state_->AsWeakPtr(),
             scoped_refptr<CryptohomeAuthenticator>(this),
             false /* ephemeral */, false /* create_if_nonexistent */);
}

void CryptohomeAuthenticator::CompleteLogin(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  DCHECK(user_context->GetUserType() == user_manager::USER_TYPE_REGULAR ||
         user_context->GetUserType() == user_manager::USER_TYPE_CHILD ||
         user_context->GetUserType() ==
             user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  current_state_ = std::make_unique<AuthAttemptState>(std::move(user_context));

  // Reset the verified flag.
  owner_is_verified_ = false;
  if (local_state_.get()) {
    user_manager::KnownUser known_user(local_state_);
    if (!known_user.UserExists(current_state_->user_context->GetAccountId())) {
      // Save logged in user into local state as early as possible.
      known_user.SaveKnownUser(current_state_->user_context->GetAccountId());
    }
  }

  StartMount(current_state_->AsWeakPtr(),
             scoped_refptr<CryptohomeAuthenticator>(this),
             false /* ephemeral */, false /* create_if_nonexistent */);

  // For login completion from extension, we just need to resolve the current
  // auth attempt state, the rest of OAuth related tasks will be done in
  // parallel.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CryptohomeAuthenticator::ResolveLoginCompletionStatus,
                     this));
}

void CryptohomeAuthenticator::LoginOffTheRecord() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  current_state_ =
      std::make_unique<AuthAttemptState>(std::make_unique<UserContext>(
          user_manager::USER_TYPE_GUEST, user_manager::GuestAccountId()));
  remove_user_data_on_failure_ = false;
  ephemeral_mount_attempted_ = true;
  MountGuestAndGetHash(current_state_->AsWeakPtr(),
                       scoped_refptr<CryptohomeAuthenticator>(this));
}

void CryptohomeAuthenticator::LoginAsPublicSession(
    const UserContext& user_context) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(user_manager::USER_TYPE_PUBLIC_ACCOUNT, user_context.GetUserType());

  // Set the cryptohome key label, as cryptohome requires a non-empty label to
  // be specified for the new mount creation requests, regardless of whether the
  // request is for a Public Session (which uses an ephemeral mount with an
  // empty password) or for a normal user.
  // TODO(crbug.com/826417): Introduce a separate constant for the Public
  // Session key label.
  auto new_user_context = std::make_unique<UserContext>(user_context);
  DCHECK(user_context.GetKey()->GetLabel().empty());
  new_user_context->GetKey()->SetLabel(kCryptohomeGaiaKeyLabel);

  current_state_ =
      std::make_unique<AuthAttemptState>(std::move(new_user_context));
  remove_user_data_on_failure_ = false;
  ephemeral_mount_attempted_ = true;
  StartMount(current_state_->AsWeakPtr(),
             scoped_refptr<CryptohomeAuthenticator>(this), true /* ephemeral */,
             true /* create_if_nonexistent */);
}

void CryptohomeAuthenticator::LoginAsKioskAccount(
    const AccountId& app_account_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  current_state_ =
      std::make_unique<AuthAttemptState>(std::make_unique<UserContext>(
          user_manager::USER_TYPE_KIOSK_APP, app_account_id));

  remove_user_data_on_failure_ = true;
  MountPublic(current_state_->AsWeakPtr(),
              scoped_refptr<CryptohomeAuthenticator>(this),
              false);  // force_dircrypto_if_available
}

void CryptohomeAuthenticator::LoginAsArcKioskAccount(
    const AccountId& app_account_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  current_state_ =
      std::make_unique<AuthAttemptState>(std::make_unique<UserContext>(
          user_manager::USER_TYPE_ARC_KIOSK_APP, app_account_id));

  remove_user_data_on_failure_ = true;
  MountPublic(current_state_->AsWeakPtr(),
              scoped_refptr<CryptohomeAuthenticator>(this),
              true);  // force_dircrypto_if_available
}

void CryptohomeAuthenticator::LoginAsWebKioskAccount(
    const AccountId& app_account_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  current_state_ =
      std::make_unique<AuthAttemptState>(std::make_unique<UserContext>(
          user_manager::USER_TYPE_WEB_KIOSK_APP, app_account_id));

  remove_user_data_on_failure_ = true;
  MountPublic(current_state_->AsWeakPtr(),
              scoped_refptr<CryptohomeAuthenticator>(this),
              false);  // force_dircrypto_if_available
}

void CryptohomeAuthenticator::OnAuthSuccess() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  VLOG(1) << "Login success";
  // Send notification of success
  chromeos::LoginEventRecorder::Get()->RecordAuthenticationSuccess();
  {
    base::AutoLock for_this_block(success_lock_);
    already_reported_success_ = true;
  }
  if (consumer_)
    consumer_->OnAuthSuccess(*current_state_->user_context);
}

void CryptohomeAuthenticator::OnOffTheRecordAuthSuccess() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  chromeos::LoginEventRecorder::Get()->RecordAuthenticationSuccess();
  if (consumer_)
    consumer_->OnOffTheRecordAuthSuccess();
}

void CryptohomeAuthenticator::OnPasswordChangeDetected() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (consumer_)
    consumer_->OnPasswordChangeDetected(*current_state_->user_context);
}

void CryptohomeAuthenticator::OnOldEncryptionDetected(
    bool has_incomplete_migration) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (consumer_) {
    consumer_->OnOldEncryptionDetected(*current_state_->user_context,
                                       has_incomplete_migration);
  }
}

// Callback invoked when UnmountEx returns.
void CryptohomeAuthenticator::OnUnmountEx(
    absl::optional<user_data_auth::UnmountReply> reply) {
  if (ReplyToMountError(reply) != cryptohome::MOUNT_ERROR_NONE)
    LOGIN_LOG(ERROR) << "Couldn't unmount user's homedir";
  OnAuthFailure(AuthFailure(AuthFailure::OWNER_REQUIRED));
}

void CryptohomeAuthenticator::OnAuthFailure(const AuthFailure& error) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // OnAuthFailure will be called again with the same |error|
  // after the cryptohome has been removed.
  if (remove_user_data_on_failure_) {
    delayed_login_failure_ = error;
    RemoveEncryptedData();
    return;
  }
  chromeos::LoginEventRecorder::Get()->RecordAuthenticationFailure();
  LOGIN_LOG(ERROR) << "Login failed: " << error.GetErrorString();
  if (consumer_)
    consumer_->OnAuthFailure(error);
}

void CryptohomeAuthenticator::MigrateKey(const UserContext& user_context,
                                         const std::string& old_password) {
  current_state_ = std::make_unique<AuthAttemptState>(
      std::make_unique<UserContext>(user_context));
  RecoverEncryptedData(std::make_unique<UserContext>(user_context),
                       old_password);
}

void CryptohomeAuthenticator::RecoverEncryptedData(
    std::unique_ptr<UserContext> user_context,
    const std::string& old_password) {
  migrate_attempted_ = true;
  current_state_->ResetCryptohomeStatus();
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &Migrate, current_state_->AsWeakPtr(),
      scoped_refptr<CryptohomeAuthenticator>(this), true, old_password));
}

void CryptohomeAuthenticator::RemoveEncryptedData() {
  remove_attempted_ = true;
  current_state_->ResetCryptohomeStatus();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Remove, current_state_->AsWeakPtr(),
                                scoped_refptr<CryptohomeAuthenticator>(this)));
}

void CryptohomeAuthenticator::ResyncEncryptedData(
    std::unique_ptr<UserContext> user_context) {
  resync_attempted_ = true;
  current_state_->ResetCryptohomeStatus();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Remove, current_state_->AsWeakPtr(),
                                scoped_refptr<CryptohomeAuthenticator>(this)));
}

bool CryptohomeAuthenticator::VerifyOwner() {
  if (owner_is_verified_)
    return true;
  // Check if policy data is fine and continue in safe mode if needed.
  if (!safe_mode_delegate_->IsSafeMode()) {
    // Now we can continue with the login and report mount success.
    user_can_login_ = true;
    owner_is_verified_ = true;
    return true;
  }

  safe_mode_delegate_->CheckSafeModeOwnership(
      current_state_->user_context->GetUserIDHash(),
      base::BindOnce(&CryptohomeAuthenticator::OnOwnershipChecked, this));
  return false;
}

void CryptohomeAuthenticator::OnOwnershipChecked(bool is_owner) {
  // Now we can check if this user is the owner.
  user_can_login_ = is_owner;
  owner_is_verified_ = true;
  Resolve();
}

void CryptohomeAuthenticator::Resolve() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  bool create_if_nonexistent = false;
  CryptohomeAuthenticator::AuthState state = ResolveState();
  VLOG(1) << "Resolved state to " << state << " (" << AuthStateToString(state)
          << ")";
  switch (state) {
    case CONTINUE:
    case POSSIBLE_PW_CHANGE:
    case NO_MOUNT:
      // These are intermediate states; we need more info from a request that
      // is still pending.
      break;
    case FAILED_MOUNT:
      // In this case, whether login succeeded or not, we can't log
      // the user in because their data is horked.  So, override with
      // the appropriate failure.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure, this,
                         AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME)));
      break;
    case FAILED_REMOVE:
      // In this case, we tried to remove the user's old cryptohome at their
      // request, and the remove failed.
      remove_user_data_on_failure_ = false;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure, this,
                         AuthFailure(AuthFailure::DATA_REMOVAL_FAILED)));
      break;
    case FAILED_TMPFS:
      // In this case, we tried to mount a tmpfs for guest and failed.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure, this,
                         AuthFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS)));
      break;
    case FAILED_TPM:
      // In this case, we tried to create/mount cryptohome and failed
      // because of the critical TPM error.
      // Chrome will notify user and request reboot.
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure,
                                    this, AuthFailure(AuthFailure::TPM_ERROR)));
      break;
    case FAILED_USERNAME_HASH:
      // In this case, we failed the GetSanitizedUsername request to
      // cryptohomed. This can happen for any login attempt.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure, this,
                         AuthFailure(AuthFailure::USERNAME_HASH_FAILED)));
      break;
    case REMOVED_DATA_AFTER_FAILURE:
      remove_user_data_on_failure_ = false;
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure,
                                    this, delayed_login_failure_));
      break;
    case CREATE_NEW:
      create_if_nonexistent = true;
      [[fallthrough]];
    case RECOVER_MOUNT:
      current_state_->ResetCryptohomeStatus();
      StartMount(current_state_->AsWeakPtr(),
                 scoped_refptr<CryptohomeAuthenticator>(this),
                 false /*ephemeral*/, create_if_nonexistent);
      break;
    case NEED_OLD_PW:
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnPasswordChangeDetected,
                         this));
      break;
    case ONLINE_FAILED:
    case NEED_NEW_PW:
    case HAVE_NEW_PW:
    case UNLOCK:
    case LOGIN_FAILED:
      NOTREACHED() << "Using obsolete ClientLogin code path.";
      break;
    case OFFLINE_LOGIN:
      VLOG(2) << "Offline login";
      [[fallthrough]];
    case ONLINE_LOGIN:
      VLOG(2) << "Online login";
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthSuccess, this));
      break;
    case GUEST_LOGIN:
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnOffTheRecordAuthSuccess,
                         this));
      break;
    case KIOSK_ACCOUNT_LOGIN:
    case PUBLIC_ACCOUNT_LOGIN:
      current_state_->user_context->SetIsUsingOAuth(false);
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthSuccess, this));
      break;
    case OWNER_REQUIRED: {
      current_state_->ResetCryptohomeStatus();
      UserDataAuthClient::Get()->Unmount(
          user_data_auth::UnmountRequest(),
          base::BindOnce(&CryptohomeAuthenticator::OnUnmountEx, this));
      break;
    }
    case FAILED_OLD_ENCRYPTION:
    case FAILED_PREVIOUS_MIGRATION_INCOMPLETE:
      // In this case, we tried to create/mount cryptohome and failed
      // because the file system is encrypted in old format.
      // Chrome will show a screen which asks user to migrate the encryption.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnOldEncryptionDetected,
                         this, state == FAILED_PREVIOUS_MIGRATION_INCOMPLETE));
      break;
    case OFFLINE_NO_MOUNT:
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure, this,
                         AuthFailure(AuthFailure::MISSING_CRYPTOHOME)));
      break;
    case TPM_UPDATE_REQUIRED:
      current_state_->ResetCryptohomeStatus();
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure, this,
                         AuthFailure(AuthFailure::TPM_UPDATE_REQUIRED)));
      break;
    case OFFLINE_MOUNT_UNRECOVERABLE:
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CryptohomeAuthenticator::OnAuthFailure, this,
                         AuthFailure(AuthFailure::UNRECOVERABLE_CRYPTOHOME)));
      break;
    default:
      NOTREACHED();
      break;
  }
}

CryptohomeAuthenticator::~CryptohomeAuthenticator() = default;

CryptohomeAuthenticator::AuthState CryptohomeAuthenticator::ResolveState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // If we haven't mounted the user's home dir yet or
  // haven't got sanitized username value, we can't be done.
  // We never get past here if any of these two cryptohome ops is still pending.
  // This is an important invariant.
  if (!current_state_->cryptohome_complete() ||
      !current_state_->username_hash_obtained()) {
    return CONTINUE;
  }

  AuthState state = CONTINUE;

  if (current_state_->cryptohome_code() == cryptohome::MOUNT_ERROR_NONE &&
      current_state_->username_hash_valid()) {
    state = ResolveCryptohomeSuccessState();
  } else {
    state = ResolveCryptohomeFailureState();
    LOGIN_LOG(ERROR) << "Cryptohome failure: "
                     << "state(AuthState)=" << state
                     << ", code(cryptohome::MountError)="
                     << current_state_->cryptohome_code();
  }

  DCHECK(current_state_->cryptohome_complete());  // Ensure invariant holds.
  migrate_attempted_ = false;
  remove_attempted_ = false;
  resync_attempted_ = false;
  ephemeral_mount_attempted_ = false;

  if (state != POSSIBLE_PW_CHANGE && state != NO_MOUNT &&
      state != OFFLINE_LOGIN)
    return state;

  if (current_state_->online_complete()) {
    // Online attempt succeeded as well, so combine the results.
    return ResolveOnlineSuccessState(state);
  }
  // if online isn't complete yet, just return the offline result.
  return state;
}

CryptohomeAuthenticator::AuthState
CryptohomeAuthenticator::ResolveCryptohomeFailureState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (remove_attempted_ || resync_attempted_)
    return FAILED_REMOVE;
  if (ephemeral_mount_attempted_)
    return FAILED_TMPFS;
  if (migrate_attempted_)
    return NEED_OLD_PW;

  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    // Critical TPM error detected, reboot needed.
    return FAILED_TPM;
  }

  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_OLD_ENCRYPTION) {
    return FAILED_OLD_ENCRYPTION;
  }
  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_PREVIOUS_MIGRATION_INCOMPLETE) {
    return FAILED_PREVIOUS_MIGRATION_INCOMPLETE;
  }
  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_TPM_UPDATE_REQUIRED) {
    return TPM_UPDATE_REQUIRED;
  }

  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_VAULT_UNRECOVERABLE) {
    // Surface up if the mount attempt failed because the vault is
    // unrecoverable.
    return OFFLINE_MOUNT_UNRECOVERABLE;
  }

  // Return intermediate states in the following case:
  // when there is an online result to use;
  // This is the case after user finishes Gaia login;
  if (current_state_->online_complete()) {
    if (current_state_->cryptohome_code() ==
        cryptohome::MOUNT_ERROR_KEY_FAILURE) {
      // If we tried a mount but they used the wrong key, we may need to
      // ask the user for their old password.  We'll only know once we've
      // done the online check.
      return POSSIBLE_PW_CHANGE;
    }
    if (current_state_->cryptohome_code() ==
        cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST) {
      // If we tried a mount but the user did not exist, then we should wait
      // for online login to succeed and try again with the "create" flag set.
      return NO_MOUNT;
    }
  } else if (current_state_->cryptohome_code() ==
             cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST) {
    // If we tried a mount but the user did not exist in the offline flow,
    // surface this as an error.
    return OFFLINE_NO_MOUNT;
  }

  if (!current_state_->username_hash_valid())
    return FAILED_USERNAME_HASH;

  return FAILED_MOUNT;
}

CryptohomeAuthenticator::AuthState
CryptohomeAuthenticator::ResolveCryptohomeSuccessState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (resync_attempted_)
    return CREATE_NEW;
  if (remove_attempted_)
    return REMOVED_DATA_AFTER_FAILURE;
  if (migrate_attempted_)
    return RECOVER_MOUNT;

  const user_manager::UserType user_type =
      current_state_->user_context->GetUserType();
  if (user_type == user_manager::USER_TYPE_GUEST)
    return GUEST_LOGIN;
  if (user_type == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return PUBLIC_ACCOUNT_LOGIN;
  if (user_type == user_manager::USER_TYPE_KIOSK_APP)
    return KIOSK_ACCOUNT_LOGIN;

  if (!VerifyOwner())
    return CONTINUE;
  return user_can_login_ ? OFFLINE_LOGIN : OWNER_REQUIRED;
}

CryptohomeAuthenticator::AuthState
CryptohomeAuthenticator::ResolveOnlineSuccessState(
    CryptohomeAuthenticator::AuthState offline_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  switch (offline_state) {
    case POSSIBLE_PW_CHANGE:
      return NEED_OLD_PW;
    case NO_MOUNT:
      return CREATE_NEW;
    case OFFLINE_LOGIN:
      return ONLINE_LOGIN;
    default:
      NOTREACHED();
      return offline_state;
  }
}

void CryptohomeAuthenticator::ResolveLoginCompletionStatus() {
  // Shortcut online state resolution process.
  current_state_->RecordOnlineLoginComplete();
  Resolve();
}

void CryptohomeAuthenticator::SetOwnerState(bool owner_check_finished,
                                            bool check_result) {
  owner_is_verified_ = owner_check_finished;
  user_can_login_ = check_result;
}

}  // namespace ash
