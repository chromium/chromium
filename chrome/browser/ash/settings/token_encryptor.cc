// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/token_encryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "crypto/encryptor.h"
#include "crypto/nss_util.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

namespace ash {

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
    std::string_view token) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return std::string(token);

  if (!system_salt_key_) {
    LOG(WARNING) << "System salt key is not available for encrypt.";
    return std::string();
  }

  // Encrypt the token using the system salt as the key and a nonce as the
  // counter.
  crypto::Encryptor encryptor;
  if (!encryptor.Init(system_salt_key_.get(), crypto::Encryptor::CTR,
                      std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }
  std::array<uint8_t, kNonceSize> nonce;
  crypto::RandBytes(nonce);
  CHECK(encryptor.SetCounter(nonce));
  std::string encoded_token;
  if (!encryptor.Encrypt(token, &encoded_token)) {
    LOG(WARNING) << "Failed to encrypt token.";
    return std::string();
  }

  // Return a concatenation of the nonce (counter) and the encrypted data, both
  // hex-encoded.
  return base::ToLowerASCII(base::HexEncode(nonce) +
                            base::HexEncode(encoded_token));
}

std::string CryptohomeTokenEncryptor::DecryptWithSystemSalt(
    std::string_view encrypted_token_hex) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return std::string(encrypted_token_hex);
  }

  if (!system_salt_key_) {
    LOG(WARNING) << "System salt key is not available for decrypt.";
    return std::string();
  }

  // Convert the encrypted token from hex to binary and then split out the
  // counter at the start from the rest of the payload.
  std::string encrypted_token;
  if (!base::HexStringToString(encrypted_token_hex, &encrypted_token)) {
    LOG(WARNING) << "Corrupt encrypted token found.";
    return std::string();
  }
  if (encrypted_token.size() < kNonceSize) {
    LOG(WARNING) << "Corrupt encrypted token found, too short.";
    return std::string();
  }
  std::string_view encrypted_token_view = encrypted_token;
  std::string_view counter = encrypted_token_view.substr(0, kNonceSize);
  std::string_view payload = encrypted_token_view.substr(kNonceSize);

  // Use the salt+nonce to decrypt the
  crypto::Encryptor encryptor;
  if (!encryptor.Init(system_salt_key_.get(), crypto::Encryptor::CTR,
                      std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }
  std::string token;
  CHECK(encryptor.SetCounter(counter));
  if (!encryptor.Decrypt(payload, &token)) {
    LOG(WARNING) << "Failed to decrypt token.";
    return std::string();
  }
  return token;
}

std::string CryptohomeTokenEncryptor::WeakEncryptWithSystemSalt(
    const std::string& token) {
  // Only tests should ever use this.
  CHECK_IS_TEST();

  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return token;
  }

  if (!system_salt_key_) {
    LOG(WARNING) << "System salt key is not available for encrypt.";
    return std::string();
  }

  // Encrypt the token using the system salt as both the key and the counter.
  // Note that using the salt for both of these things is problematic, which is
  // why this encryption is "weak".
  crypto::Encryptor encryptor;
  if (!encryptor.Init(system_salt_key_.get(), crypto::Encryptor::CTR,
                      std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }
  std::string nonce = system_salt_.substr(0, kNonceSize);
  std::string encoded_token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Encrypt(token, &encoded_token)) {
    LOG(WARNING) << "Failed to encrypt token.";
    return std::string();
  }

  return base::ToLowerASCII(base::HexEncode(encoded_token));
}

std::string CryptohomeTokenEncryptor::WeakDecryptWithSystemSalt(
    const std::string& encrypted_token_hex) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return encrypted_token_hex;
  }

  if (!system_salt_key_) {
    LOG(WARNING) << "System salt key is not available for decrypt.";
    return std::string();
  }

  std::string encrypted_token;
  if (!base::HexStringToString(encrypted_token_hex, &encrypted_token)) {
    LOG(WARNING) << "Corrupt encrypted token found.";
    return std::string();
  }

  crypto::Encryptor encryptor;
  if (!encryptor.Init(system_salt_key_.get(), crypto::Encryptor::CTR,
                      std::string())) {
    LOG(WARNING) << "Failed to initialize Encryptor.";
    return std::string();
  }

  std::string nonce = system_salt_.substr(0, kNonceSize);
  std::string token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Decrypt(encrypted_token, &token)) {
    LOG(WARNING) << "Failed to decrypt token.";
    return std::string();
  }
  return token;
}

std::unique_ptr<crypto::SymmetricKey> CryptohomeTokenEncryptor::PassphraseToKey(
    const std::string& passphrase,
    const std::string& salt) {
  return crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
      crypto::SymmetricKey::AES, passphrase, salt, 1000, 256);
}

}  // namespace ash
