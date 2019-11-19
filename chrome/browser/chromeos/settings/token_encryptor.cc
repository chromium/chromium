// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/token_encryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "crypto/encryptor.h"
#include "crypto/nss_util.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

namespace chromeos {

namespace {
const size_t kNonceSize = 16;
}  // namespace

CryptohomeTokenEncryptor::CryptohomeTokenEncryptor(
    const std::string& system_salt)
    : system_salt_(system_salt) {
  DCHECK(!system_salt.empty());
  // TODO(davidroche): should this use the system salt for both the password
  // and the salt value, or should this use a separate salt value?
  system_salt_key_ = PassphraseToKey(system_salt_, system_salt_);
}

CryptohomeTokenEncryptor::~CryptohomeTokenEncryptor() {
}

std::string CryptohomeTokenEncryptor::EncryptWithSystemSalt(
    const std::string& token) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return token;

  if (!system_salt_key_) {
    LOG(WARNING) << "System salt key is not available for encrypt.";
    return std::string();
  }
  return EncryptTokenWithKey(system_salt_key_.get(),
                             system_salt_,
                             token);
}

std::string CryptohomeTokenEncryptor::DecryptWithSystemSalt(
    const std::string& encrypted_token_hex) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return encrypted_token_hex;

  if (!system_salt_key_) {
    LOG(WARNING) << "System salt key is not available for decrypt.";
    return std::string();
  }
  return DecryptTokenWithKey(system_salt_key_.get(),
                             system_salt_,
                             encrypted_token_hex);
}

std::unique_ptr<crypto::SymmetricKey> CryptohomeTokenEncryptor::PassphraseToKey(
    const std::string& passphrase,
    const std::string& salt) {
  return crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
      crypto::SymmetricKey::AES, passphrase, salt, 1000, 256);
}

std::string CryptohomeTokenEncryptor::EncryptTokenWithKey(
    const crypto::SymmetricKey* key,
    const std::string& salt,
    const std::string& token) {
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }
  std::string nonce = salt.substr(0, kNonceSize);
  std::string encoded_token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Encrypt(token, &encoded_token)) {
    LOG(WARNING) << "Failed to encrypt token.";
    return std::string();
  }

  return base::ToLowerASCII(
      base::HexEncode(reinterpret_cast<const void*>(encoded_token.data()),
                      encoded_token.size()));
}

std::string CryptohomeTokenEncryptor::DecryptTokenWithKey(
    const crypto::SymmetricKey* key,
    const std::string& salt,
    const std::string& encrypted_token_hex) {
  std::string encrypted_token;
  if (!base::HexStringToString(encrypted_token_hex, &encrypted_token)) {
    LOG(WARNING) << "Corrupt encrypted token found.";
    return std::string();
  }

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }

  std::string nonce = salt.substr(0, kNonceSize);
  std::string token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Decrypt(encrypted_token, &token)) {
    LOG(WARNING) << "Failed to decrypt token.";
    return std::string();
  }
  return token;
}

}  // namespace chromeos
