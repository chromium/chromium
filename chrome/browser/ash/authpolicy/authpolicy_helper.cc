// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/authpolicy/data_pipe_utils.h"
#include "chromeos/ash/components/dbus/authpolicy/authpolicy_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/account_id/account_id.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"

namespace ash {

namespace {

constexpr char kDCPrefix[] = "DC=";
constexpr char kOUPrefix[] = "OU=";

bool ParseDomainAndOU(const std::string& distinguished_name,
                      authpolicy::JoinDomainRequest* request) {
  std::string machine_domain;
  std::vector<std::string> split_dn =
      base::SplitString(distinguished_name, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& str : split_dn) {
    if (base::StartsWith(str, kOUPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      *request->add_machine_ou() = str.substr(strlen(kOUPrefix));
    } else if (base::StartsWith(str, kDCPrefix,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      if (!machine_domain.empty())
        machine_domain.append(".");
      machine_domain.append(str.substr(strlen(kDCPrefix)));
    } else {
      return false;
    }
  }
  if (!machine_domain.empty())
    request->set_machine_domain(machine_domain);
  return true;
}

std::string DoDecrypt(const std::string& encrypted_data,
                      const std::string& password) {
  constexpr char error_msg[] = "Failed to decrypt data";
  const size_t kSaltSize = 32;
  const size_t kSignatureSize = 32;
  if (encrypted_data.size() <= kSaltSize + kSignatureSize) {
    LOG(ERROR) << error_msg;
    return std::string();
  }

  const std::string salt = encrypted_data.substr(0, kSaltSize);
  const std::string signature =
      encrypted_data.substr(kSaltSize, kSignatureSize);
  const std::string ciphertext =
      encrypted_data.substr(kSaltSize + kSignatureSize);

  // Derive AES key, AES IV and HMAC key from password.
  const size_t kAesKeySize = 32;
  const size_t kAesIvSize = 16;
  const size_t kHmacKeySize = 32;
  const size_t kKeySize = kAesKeySize + kAesIvSize + kHmacKeySize;
  std::unique_ptr<crypto::SymmetricKey> key =
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::HMAC_SHA1, password, salt, 10000, kKeySize * 8);
  if (!key) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  DCHECK(kAesKeySize + kAesIvSize + kHmacKeySize == key->key().size());
  const char* key_data_chars = key->key().data();
  std::string aes_key(key_data_chars, kAesKeySize);
  std::string aes_iv(key_data_chars + kAesKeySize, kAesIvSize);
  std::string hmac_key(key_data_chars + kAesKeySize + kAesIvSize, kHmacKeySize);

  // Check signature.
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (kSignatureSize != hmac.DigestLength()) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  uint8_t recomputed_signature[kSignatureSize];
  if (!hmac.Init(hmac_key) ||
      !hmac.Sign(ciphertext, recomputed_signature, kSignatureSize)) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  std::string recomputed_signature_str(
      reinterpret_cast<char*>(recomputed_signature), kSignatureSize);
  if (signature != recomputed_signature_str) {
    LOG(ERROR) << error_msg;
    return std::string();
  }

  // Decrypt.
  std::unique_ptr<crypto::SymmetricKey> aes_key_obj(
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, aes_key));
  crypto::Encryptor encryptor;
  if (!encryptor.Init(aes_key_obj.get(), crypto::Encryptor::CBC, aes_iv)) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  std::string decrypted_data;
  if (!encryptor.Decrypt(ciphertext, &decrypted_data)) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  return decrypted_data;
}

}  // namespace

AuthPolicyHelper::AuthPolicyHelper() {
  AuthPolicyClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &AuthPolicyHelper::OnServiceAvailable, weak_factory_.GetWeakPtr()));
}

// static
void AuthPolicyHelper::TryAuthenticateUser(const std::string& username,
                                           const std::string& object_guid,
                                           const std::string& password) {
  authpolicy::AuthenticateUserRequest request;
  request.set_user_principal_name(username);
  request.set_account_id(object_guid);
  AuthPolicyClient::Get()->AuthenticateUser(
      request, data_pipe_utils::GetDataReadPipe(password).get(),
      base::DoNothing());
}

// static
void AuthPolicyHelper::Restart() {
  UpstartClient::Get()->RestartAuthPolicyService();
}

// static
void AuthPolicyHelper::DecryptConfiguration(const std::string& blob,
                                            const std::string& password,
                                            OnDecryptedCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DoDecrypt, blob, password), std::move(callback));
}

void AuthPolicyHelper::JoinAdDomain(const std::string& machine_name,
                                    const std::string& distinguished_name,
                                    int encryption_types,
                                    const std::string& username,
                                    const std::string& password,
                                    JoinCallback callback) {
  DCHECK(service_is_available_);
  DCHECK(!InstallAttributes::Get()->IsActiveDirectoryManaged());
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  authpolicy::JoinDomainRequest request;
  if (!ParseDomainAndOU(distinguished_name, &request)) {
    DLOG(ERROR) << "Failed to parse computer distinguished name";
    std::move(callback).Run(authpolicy::ERROR_INVALID_OU, std::string());
    return;
  }
  if (!machine_name.empty())
    request.set_machine_name(machine_name);
  DCHECK(authpolicy::KerberosEncryptionTypes_IsValid(encryption_types));
  request.set_kerberos_encryption_types(
      static_cast<authpolicy::KerberosEncryptionTypes>(encryption_types));
  if (!username.empty())
    request.set_user_principal_name(username);
  DCHECK(!dm_token_.empty());
  request.set_dm_token(dm_token_);

  AuthPolicyClient::Get()->JoinAdDomain(
      request, data_pipe_utils::GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyHelper::OnJoinCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthPolicyHelper::AuthenticateUser(const std::string& username,
                                        const std::string& object_guid,
                                        const std::string& password,
                                        AuthCallback callback) {
  DCHECK(service_is_available_);
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  authpolicy::AuthenticateUserRequest request;
  request.set_user_principal_name(username);
  request.set_account_id(object_guid);
  AuthPolicyClient::Get()->AuthenticateUser(
      request, data_pipe_utils::GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyHelper::OnAuthCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthPolicyHelper::RefreshDevicePolicy(RefreshPolicyCallback callback) {
  if (service_is_available_) {
    AuthPolicyClient::Get()->RefreshDevicePolicy(std::move(callback));
    return;
  }
  DCHECK(!device_policy_callback_);
  device_policy_callback_ = std::move(callback);
}

void AuthPolicyHelper::RefreshUserPolicy(const AccountId& account_id,
                                         RefreshPolicyCallback callback) const {
  DCHECK(service_is_available_);
  AuthPolicyClient::Get()->RefreshUserPolicy(account_id, std::move(callback));
}

void AuthPolicyHelper::CancelRequestsAndRestart() {
  weak_factory_.InvalidateWeakPtrs();
  dm_token_.clear();
  AuthPolicyHelper::Restart();
  service_is_available_ = false;
  AuthPolicyClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &AuthPolicyHelper::OnServiceAvailable, weak_factory_.GetWeakPtr()));
}

void AuthPolicyHelper::OnServiceAvailable(bool service_is_available) {
  DCHECK(service_is_available);
  service_is_available_ = true;
  if (device_policy_callback_) {
    AuthPolicyClient::Get()->RefreshDevicePolicy(
        std::move(device_policy_callback_));
  }
}

void AuthPolicyHelper::OnJoinCallback(JoinCallback callback,
                                      authpolicy::ErrorType error,
                                      const std::string& machine_domain) {
  DCHECK(!InstallAttributes::Get()->IsActiveDirectoryManaged());
  if (error != authpolicy::ERROR_NONE) {
    std::move(callback).Run(error, machine_domain);
    return;
  }
  AuthPolicyClient::Get()->RefreshDevicePolicy(base::BindOnce(
      &AuthPolicyHelper::OnFirstPolicyRefreshCallback,
      weak_factory_.GetWeakPtr(), std::move(callback), machine_domain));
}

void AuthPolicyHelper::OnFirstPolicyRefreshCallback(
    JoinCallback callback,
    const std::string& machine_domain,
    authpolicy::ErrorType error) {
  DCHECK(!InstallAttributes::Get()->IsActiveDirectoryManaged());
  // First policy refresh happens before device is locked. So policy store
  // should not succeed. The error means that authpolicyd cached device policy
  // and stores it in the next call to RefreshDevicePolicy in STEP_STORE_POLICY.
  DCHECK(error != authpolicy::ERROR_NONE);
  if (error == authpolicy::ERROR_DEVICE_POLICY_CACHED_BUT_NOT_SENT)
    error = authpolicy::ERROR_NONE;
  std::move(callback).Run(error, machine_domain);
}

void AuthPolicyHelper::OnAuthCallback(
    AuthCallback callback,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  DCHECK_NE(authpolicy::ERROR_DBUS_FAILURE, error);
  std::move(callback).Run(error, account_info);
}

AuthPolicyHelper::~AuthPolicyHelper() = default;

}  // namespace ash
