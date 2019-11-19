// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_constants.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/cryptohome/signed_secret.pb.h"
#include "chromeos/login/auth/key.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"

namespace chromeos {

namespace {

// Byte size of hash salt.
const unsigned kSaltSize = 32;

// Size of key signature.
const unsigned kHMACKeySizeInBits = 256;
const int kSignatureLength = 32;

// Size of master key (in bytes).
const int kMasterKeySize = 32;

std::string CreateSalt() {
  char result[kSaltSize];
  crypto::RandBytes(&result, sizeof(result));
  return base::ToLowerASCII(
      base::HexEncode(reinterpret_cast<const void*>(result), sizeof(result)));
}

std::string BuildRawHMACKey() {
  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES,
                                              kHMACKeySizeInBits));
  std::string result;
  base::Base64Encode(key->key(), &result);
  return result;
}

base::DictionaryValue* LoadPasswordData(base::FilePath profile_dir) {
  JSONFileValueDeserializer deserializer(
      profile_dir.Append(kPasswordUpdateFile));
  std::string error_message;
  int error_code = JSONFileValueDeserializer::JSON_NO_ERROR;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (JSONFileValueDeserializer::JSON_NO_ERROR != error_code) {
    LOG(ERROR) << "Could not deserialize password data, error = " << error_code
               << " / " << error_message;
    return NULL;
  }
  base::DictionaryValue* result;
  if (!value->GetAsDictionary(&result)) {
    LOG(ERROR) << "Stored password data is not a dictionary";
    return NULL;
  }
  ignore_result(value.release());
  return result;
}

void OnPasswordDataLoaded(
    const SupervisedUserAuthentication::PasswordDataCallback& success_callback,
    const base::Closure& failure_callback,
    base::DictionaryValue* value) {
  if (!value) {
    failure_callback.Run();
    return;
  }
  success_callback.Run(value);
  delete value;
}

}  // namespace

SupervisedUserAuthentication::SupervisedUserAuthentication(
    SupervisedUserManager* owner)
    : owner_(owner), stable_schema_(SCHEMA_SALT_HASHED) {}

SupervisedUserAuthentication::~SupervisedUserAuthentication() {}

SupervisedUserAuthentication::Schema
SupervisedUserAuthentication::GetStableSchema() {
  return stable_schema_;
}

UserContext SupervisedUserAuthentication::TransformKey(
    const UserContext& context) {
  UserContext result = context;
  int user_schema = GetPasswordSchema(context.GetAccountId().GetUserEmail());
  if (user_schema == SCHEMA_PLAIN)
    return result;

  if (user_schema == SCHEMA_SALT_HASHED) {
    base::DictionaryValue holder;
    std::string salt;
    owner_->GetPasswordInformation(context.GetAccountId().GetUserEmail(),
                                   &holder);
    holder.GetStringWithoutPathExpansion(kSalt, &salt);
    DCHECK(!salt.empty());
    Key* const key = result.GetKey();
    key->Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
    key->SetLabel(kCryptohomeSupervisedUserKeyLabel);
    result.SetIsUsingOAuth(false);
    return result;
  }
  NOTREACHED() << "Unknown password schema for "
               << context.GetAccountId().GetUserEmail();
  return context;
}

bool SupervisedUserAuthentication::FillDataForNewUser(
    const std::string& user_id,
    const std::string& password,
    base::DictionaryValue* password_data,
    base::DictionaryValue* extra_data) {
  Schema schema = stable_schema_;
  if (schema == SCHEMA_PLAIN)
    return false;

  if (schema == SCHEMA_SALT_HASHED) {
    password_data->SetKey(kSchemaVersion, base::Value(schema));
    std::string salt = CreateSalt();
    password_data->SetKey(kSalt, base::Value(salt));
    int revision = kMinPasswordRevision;
    password_data->SetKey(kPasswordRevision, base::Value(revision));
    Key key(password);
    key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
    const std::string salted_password = key.GetSecret();
    const std::string base64_signature_key = BuildRawHMACKey();
    const std::string base64_signature =
        BuildPasswordSignature(salted_password, revision, base64_signature_key);
    password_data->SetKey(kEncryptedPassword, base::Value(salted_password));
    password_data->SetKey(kPasswordSignature, base::Value(base64_signature));

    extra_data->SetKey(kPasswordEncryptionKey, base::Value(BuildRawHMACKey()));
    extra_data->SetKey(kPasswordSignatureKey,
                       base::Value(base64_signature_key));
    return true;
  }
  NOTREACHED();
  return false;
}

std::string SupervisedUserAuthentication::GenerateMasterKey() {
  char master_key_bytes[kMasterKeySize];
  crypto::RandBytes(&master_key_bytes, sizeof(master_key_bytes));
  return base::ToLowerASCII(
      base::HexEncode(reinterpret_cast<const void*>(master_key_bytes),
                      sizeof(master_key_bytes)));
}

void SupervisedUserAuthentication::StorePasswordData(
    const std::string& user_id,
    const base::DictionaryValue& password_data) {
  base::DictionaryValue holder;
  owner_->GetPasswordInformation(user_id, &holder);
  const base::Value* value;
  if (password_data.GetWithoutPathExpansion(kSchemaVersion, &value))
    holder.SetWithoutPathExpansion(kSchemaVersion, value->CreateDeepCopy());
  if (password_data.GetWithoutPathExpansion(kSalt, &value))
    holder.SetWithoutPathExpansion(kSalt, value->CreateDeepCopy());
  if (password_data.GetWithoutPathExpansion(kPasswordRevision, &value))
    holder.SetWithoutPathExpansion(kPasswordRevision, value->CreateDeepCopy());
  owner_->SetPasswordInformation(user_id, &holder);
}

SupervisedUserAuthentication::Schema
SupervisedUserAuthentication::GetPasswordSchema(const std::string& user_id) {
  base::DictionaryValue holder;

  owner_->GetPasswordInformation(user_id, &holder);
  // Default version.
  int schema_version_index;
  Schema schema_version = SCHEMA_PLAIN;
  if (holder.GetIntegerWithoutPathExpansion(kSchemaVersion,
                                            &schema_version_index)) {
    schema_version = static_cast<Schema>(schema_version_index);
  }
  return schema_version;
}

bool SupervisedUserAuthentication::NeedPasswordChange(
    const std::string& user_id,
    const base::DictionaryValue* password_data) {
  base::DictionaryValue local;
  owner_->GetPasswordInformation(user_id, &local);
  int local_schema = SCHEMA_PLAIN;
  int local_revision = kMinPasswordRevision;
  int updated_schema = SCHEMA_PLAIN;
  int updated_revision = kMinPasswordRevision;
  local.GetIntegerWithoutPathExpansion(kSchemaVersion, &local_schema);
  local.GetIntegerWithoutPathExpansion(kPasswordRevision, &local_revision);
  password_data->GetIntegerWithoutPathExpansion(kSchemaVersion,
                                                &updated_schema);
  password_data->GetIntegerWithoutPathExpansion(kPasswordRevision,
                                                &updated_revision);
  if (updated_schema > local_schema)
    return true;
  DCHECK_EQ(updated_schema, local_schema);
  return updated_revision > local_revision;
}

void SupervisedUserAuthentication::ScheduleSupervisedPasswordChange(
    const std::string& supervised_user_id,
    const base::DictionaryValue* password_data) {
  const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
      AccountId::FromUserEmail(supervised_user_id));
  base::FilePath profile_path =
      ProfileHelper::GetProfilePathByUserIdHash(user->username_hash());
  JSONFileValueSerializer serializer(profile_path.Append(kPasswordUpdateFile));
  if (!serializer.Serialize(*password_data)) {
    LOG(ERROR) << "Failed to schedule password update for supervised user "
               << supervised_user_id;
    UMA_HISTOGRAM_ENUMERATION(
        "ManagedUsers.ChromeOS.PasswordChange",
        SupervisedUserAuthentication::PASSWORD_CHANGE_FAILED_STORE_DATA,
        SupervisedUserAuthentication::PASSWORD_CHANGE_RESULT_MAX_VALUE);
    return;
  }
  base::DictionaryValue holder;
  owner_->GetPasswordInformation(supervised_user_id, &holder);
  holder.SetBoolean(kRequirePasswordUpdate, true);
  owner_->SetPasswordInformation(supervised_user_id, &holder);
}

bool SupervisedUserAuthentication::HasScheduledPasswordUpdate(
    const std::string& user_id) {
  base::DictionaryValue holder;
  owner_->GetPasswordInformation(user_id, &holder);
  bool require_update = false;
  holder.GetBoolean(kRequirePasswordUpdate, &require_update);
  return require_update;
}

void SupervisedUserAuthentication::ClearScheduledPasswordUpdate(
    const std::string& user_id) {
  base::DictionaryValue holder;
  owner_->GetPasswordInformation(user_id, &holder);
  holder.SetBoolean(kRequirePasswordUpdate, false);
  owner_->SetPasswordInformation(user_id, &holder);
}

bool SupervisedUserAuthentication::HasIncompleteKey(
    const std::string& user_id) {
  base::DictionaryValue holder;
  owner_->GetPasswordInformation(user_id, &holder);
  bool incomplete_key = false;
  holder.GetBoolean(kHasIncompleteKey, &incomplete_key);
  return incomplete_key;
}

void SupervisedUserAuthentication::MarkKeyIncomplete(const std::string& user_id,
                                                     bool incomplete) {
  base::DictionaryValue holder;
  owner_->GetPasswordInformation(user_id, &holder);
  holder.SetBoolean(kHasIncompleteKey, incomplete);
  owner_->SetPasswordInformation(user_id, &holder);
}

void SupervisedUserAuthentication::LoadPasswordUpdateData(
    const std::string& user_id,
    const PasswordDataCallback& success_callback,
    const base::Closure& failure_callback) {
  const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
      AccountId::FromUserEmail(user_id));
  base::FilePath profile_path =
      ProfileHelper::GetProfilePathByUserIdHash(user->username_hash());
  PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&LoadPasswordData, profile_path),
      base::BindOnce(&OnPasswordDataLoaded, success_callback,
                     failure_callback));
}

std::string SupervisedUserAuthentication::BuildPasswordSignature(
    const std::string& password,
    int revision,
    const std::string& base64_signature_key) {
  ac::chrome::managedaccounts::account::Secret secret;
  secret.set_revision(revision);
  secret.set_secret(password);
  std::string buffer;
  if (!secret.SerializeToString(&buffer))
    LOG(FATAL) << "Protobuf::SerializeToString failed";
  std::string signature_key;
  base::Base64Decode(base64_signature_key, &signature_key);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(signature_key))
    LOG(FATAL) << "HMAC::Init failed";

  unsigned char out_bytes[kSignatureLength];
  if (!hmac.Sign(buffer, out_bytes, sizeof(out_bytes)))
    LOG(FATAL) << "HMAC::Sign failed";

  std::string raw_result(out_bytes, out_bytes + sizeof(out_bytes));

  std::string result;
  base::Base64Encode(raw_result, &result);
  return result;
}

}  // namespace chromeos
