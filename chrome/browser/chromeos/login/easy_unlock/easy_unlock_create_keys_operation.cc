// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_create_keys_operation.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/login/auth/key.h"
#include "crypto/encryptor.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const int kUserKeyByteSize = 16;
const int kSessionKeyByteSize = 16;

const int kEasyUnlockKeyRevision = 1;
const int kEasyUnlockKeyPrivileges =
    cryptohome::PRIV_MOUNT | cryptohome::PRIV_ADD | cryptohome::PRIV_REMOVE;

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// EasyUnlockCreateKeysOperation::ChallengeCreator

class EasyUnlockCreateKeysOperation::ChallengeCreator {
 public:
  typedef base::Callback<void(bool success)> ChallengeCreatedCallback;
  ChallengeCreator(const std::string& user_key,
                   const std::string& session_key,
                   const std::string& tpm_pub_key,
                   EasyUnlockDeviceKeyData* device,
                   const ChallengeCreatedCallback& callback);
  ~ChallengeCreator();

  void Start();

  const std::string& user_key() const { return user_key_; }

 private:
  void OnEcKeyPairGenerated(const std::string& ec_public_key,
                            const std::string& ec_private_key);
  void OnEskGenerated(const std::string& esk);
  void OnTPMPublicKeyWrapped(const std::string& wrapped_key);

  void WrapTPMPublicKey();
  void GeneratePayload();
  void OnPayloadMessageGenerated(const std::string& payload_message);
  void OnPayloadGenerated(const std::string& payload);

  void OnChallengeGenerated(const std::string& challenge);

  const std::string user_key_;
  const std::string session_key_;
  const std::string tpm_pub_key_;
  std::string wrapped_tpm_pub_key_;
  EasyUnlockDeviceKeyData* device_;
  ChallengeCreatedCallback callback_;

  std::string ec_public_key_;
  std::string esk_;

  // Owned by DBusThreadManager
  EasyUnlockClient* easy_unlock_client_;

  base::WeakPtrFactory<ChallengeCreator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChallengeCreator);
};

EasyUnlockCreateKeysOperation::ChallengeCreator::ChallengeCreator(
    const std::string& user_key,
    const std::string& session_key,
    const std::string& tpm_pub_key,
    EasyUnlockDeviceKeyData* device,
    const ChallengeCreatedCallback& callback)
    : user_key_(user_key),
      session_key_(session_key),
      tpm_pub_key_(tpm_pub_key),
      device_(device),
      callback_(callback),
      easy_unlock_client_(DBusThreadManager::Get()->GetEasyUnlockClient()) {}

EasyUnlockCreateKeysOperation::ChallengeCreator::~ChallengeCreator() {}

void EasyUnlockCreateKeysOperation::ChallengeCreator::Start() {
  easy_unlock_client_->GenerateEcP256KeyPair(base::Bind(
      &ChallengeCreator::OnEcKeyPairGenerated, weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnEcKeyPairGenerated(
    const std::string& ec_private_key,
    const std::string& ec_public_key) {
  if (ec_private_key.empty() || ec_public_key.empty()) {
    PA_LOG(ERROR) << "Easy unlock failed to generate ec key pair.";
    callback_.Run(false);
    return;
  }

  std::string device_pub_key;
  if (!base::Base64UrlDecode(device_->public_key,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &device_pub_key)) {
    PA_LOG(ERROR) << "Easy unlock failed to decode device public key.";
    callback_.Run(false);
    return;
  }

  ec_public_key_ = ec_public_key;
  easy_unlock_client_->PerformECDHKeyAgreement(
      ec_private_key, device_pub_key,
      base::Bind(&ChallengeCreator::OnEskGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnEskGenerated(
    const std::string& esk) {
  if (esk.empty()) {
    PA_LOG(ERROR) << "Easy unlock failed to generate challenge esk.";
    callback_.Run(false);
    return;
  }

  esk_ = esk;
  WrapTPMPublicKey();
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::WrapTPMPublicKey() {
  easy_unlock_client_->WrapPublicKey(
      easy_unlock::kKeyAlgorithmRSA, tpm_pub_key_,
      base::Bind(&ChallengeCreator::OnTPMPublicKeyWrapped,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnTPMPublicKeyWrapped(
    const std::string& wrapped_key) {
  if (wrapped_key.empty()) {
    PA_LOG(ERROR) << "Unable to wrap RSA key";
    callback_.Run(false);
    return;
  }
  wrapped_tpm_pub_key_ = wrapped_key;
  GeneratePayload();
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::GeneratePayload() {
  // Work around to get HeaderAndBody bytes to use as challenge payload.
  EasyUnlockClient::CreateSecureMessageOptions options;
  options.key = esk_;
  options.verification_key_id = wrapped_tpm_pub_key_;
  options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  easy_unlock_client_->CreateSecureMessage(
      session_key_, options,
      base::Bind(&ChallengeCreator::OnPayloadMessageGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnPayloadMessageGenerated(
    const std::string& payload_message) {
  EasyUnlockClient::UnwrapSecureMessageOptions options;
  options.key = esk_;
  options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  easy_unlock_client_->UnwrapSecureMessage(
      payload_message, options,
      base::Bind(&ChallengeCreator::OnPayloadGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnPayloadGenerated(
    const std::string& payload) {
  if (payload.empty()) {
    PA_LOG(ERROR) << "Easy unlock failed to generate challenge payload.";
    callback_.Run(false);
    return;
  }

  EasyUnlockClient::CreateSecureMessageOptions options;
  options.key = esk_;
  options.decryption_key_id = ec_public_key_;
  options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  easy_unlock_client_->CreateSecureMessage(
      payload, options,
      base::Bind(&ChallengeCreator::OnChallengeGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockCreateKeysOperation::ChallengeCreator::OnChallengeGenerated(
    const std::string& challenge) {
  if (challenge.empty()) {
    PA_LOG(ERROR) << "Easy unlock failed to generate challenge.";
    callback_.Run(false);
    return;
  }

  device_->challenge = challenge;
  callback_.Run(true);
}

/////////////////////////////////////////////////////////////////////////////
// EasyUnlockCreateKeysOperation

EasyUnlockCreateKeysOperation::EasyUnlockCreateKeysOperation(
    const UserContext& user_context,
    const std::string& tpm_public_key,
    const EasyUnlockDeviceKeyDataList& devices,
    const CreateKeysCallback& callback)
    : user_context_(user_context),
      tpm_public_key_(tpm_public_key),
      devices_(devices),
      callback_(callback),
      key_creation_index_(0) {
  // Must have the secret and callback.
  DCHECK(!user_context_.GetKey()->GetSecret().empty());
  DCHECK(!callback_.is_null());
}

EasyUnlockCreateKeysOperation::~EasyUnlockCreateKeysOperation() {}

void EasyUnlockCreateKeysOperation::Start() {
  key_creation_index_ = 0;
  CreateKeyForDeviceAtIndex(key_creation_index_);
}

void EasyUnlockCreateKeysOperation::CreateKeyForDeviceAtIndex(size_t index) {
  DCHECK_GE(index, 0u);
  if (index == devices_.size()) {
    callback_.Run(true);
    return;
  }

  std::string user_key;
  crypto::RandBytes(base::WriteInto(&user_key, kUserKeyByteSize + 1),
                    kUserKeyByteSize);

  std::unique_ptr<crypto::SymmetricKey> session_key(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES,
                                              kSessionKeyByteSize * 8));

  std::string iv(kSessionKeyByteSize, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(session_key.get(), crypto::Encryptor::CBC, iv)) {
    PA_LOG(ERROR) << "Easy unlock failed to init encryptor for key creation.";
    callback_.Run(false);
    return;
  }

  EasyUnlockDeviceKeyData* device = &devices_[index];
  if (!encryptor.Encrypt(user_key, &device->wrapped_secret)) {
    PA_LOG(ERROR) << "Easy unlock failed to encrypt user key for key creation.";
    callback_.Run(false);
    return;
  }

  challenge_creator_.reset(new ChallengeCreator(
      user_key, session_key->key(), tpm_public_key_, device,
      base::Bind(&EasyUnlockCreateKeysOperation::OnChallengeCreated,
                 weak_ptr_factory_.GetWeakPtr(), index)));
  challenge_creator_->Start();
}

void EasyUnlockCreateKeysOperation::OnChallengeCreated(size_t index,
                                                       bool success) {
  DCHECK_EQ(key_creation_index_, index);

  if (!success) {
    PA_LOG(ERROR) << "Easy unlock failed to create challenge for key creation.";
    callback_.Run(false);
    return;
  }

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&EasyUnlockCreateKeysOperation::OnGetSystemSalt,
                 weak_ptr_factory_.GetWeakPtr(), index));
}

void EasyUnlockCreateKeysOperation::OnGetSystemSalt(
    size_t index,
    const std::string& system_salt) {
  DCHECK_EQ(key_creation_index_, index);
  if (system_salt.empty()) {
    PA_LOG(ERROR) << "Easy unlock failed to get system salt for key creation.";
    callback_.Run(false);
    return;
  }

  Key user_key(challenge_creator_->user_key());
  user_key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  EasyUnlockDeviceKeyData* device = &devices_[index];
  auto key_def = cryptohome::KeyDefinition::CreateForPassword(
      user_key.GetSecret(), EasyUnlockKeyManager::GetKeyLabel(index),
      kEasyUnlockKeyPrivileges);
  key_def.revision = kEasyUnlockKeyRevision;
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameBluetoothAddress, device->bluetooth_address));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameBluetoothType,
      static_cast<int64_t>(device->bluetooth_type)));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNamePsk, device->psk));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNamePubKey, device->public_key));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameChallenge, device->challenge));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameWrappedSecret, device->wrapped_secret));
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameSerializedBeaconSeeds,
      device->serialized_beacon_seeds));
  // ProviderData only has std::string and int64_t fields for persistence -- use
  // the int64_t field to store this boolean. The boolean is stored as either a
  // 1 or 0 in as an int64_t.
  key_def.provider_data.push_back(cryptohome::KeyDefinition::ProviderData(
      kEasyUnlockKeyMetaNameUnlockKey,
      static_cast<int64_t>(device->unlock_key)));

  std::unique_ptr<Key> auth_key(new Key(*user_context_.GetKey()));
  if (auth_key->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN)
    auth_key->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  cryptohome::AddKeyRequest request;
  cryptohome::KeyDefinitionToKey(key_def, request.mutable_key());
  request.set_clobber_if_exists(true);

  // Create the authorization request with an empty label, in order to act as a
  // wildcard. See https://crbug.com/1002336 for more.
  cryptohome::HomedirMethods::GetInstance()->AddKeyEx(
      cryptohome::Identification(user_context_.GetAccountId()),
      cryptohome::CreateAuthorizationRequest(std::string() /* label */,
                                             auth_key->GetSecret()),
      request,
      base::Bind(&EasyUnlockCreateKeysOperation::OnKeyCreated,
                 weak_ptr_factory_.GetWeakPtr(), index, user_key));
}

void EasyUnlockCreateKeysOperation::OnKeyCreated(
    size_t index,
    const Key& user_key,
    bool success,
    cryptohome::MountError return_code) {
  DCHECK_EQ(key_creation_index_, index);

  if (!success) {
    PA_LOG(ERROR) << "Easy unlock failed to create key, code=" << return_code;
    callback_.Run(false);
    return;
  }

  // If the key associated with the current context changed (i.e. in the case
  // the current signin flow was Easy signin), update the user context.
  if (user_context_.GetAuthFlow() == UserContext::AUTH_FLOW_EASY_UNLOCK &&
      user_context_.GetKey()->GetLabel() ==
          EasyUnlockKeyManager::GetKeyLabel(key_creation_index_)) {
    user_context_.SetKey(user_key);
  }

  ++key_creation_index_;
  CreateKeyForDeviceAtIndex(key_creation_index_);
}

}  // namespace chromeos
