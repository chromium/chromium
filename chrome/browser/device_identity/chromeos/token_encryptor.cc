// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/chromeos/token_encryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "crypto/aes_ctr.h"
#include "crypto/kdf.h"
#include "crypto/random.h"
#include "crypto/subtle_passkey.h"

namespace ash {

namespace {

constexpr crypto::kdf::Pbkdf2HmacSha1Params kPbkdf2Params = {
    .iterations = 1000,
};

}  // namespace

CryptohomeTokenEncryptor::CryptohomeTokenEncryptor(
    const std::string& system_salt) {
  CHECK(!system_salt.empty());

  auto salt = base::as_byte_span(system_salt);
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(kPbkdf2Params, salt, salt, key_,
                                       crypto::SubtlePassKey{});
  base::span(nonce_).copy_from(salt.first<kNonceSize>());
}

CryptohomeTokenEncryptor::~CryptohomeTokenEncryptor() {}

std::string CryptohomeTokenEncryptor::EncryptWithSystemSalt(
    std::string_view token) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return std::string(token);
  }

  std::array<uint8_t, kNonceSize> nonce;
  crypto::RandBytes(nonce);

  auto ciphertext =
      crypto::aes_ctr::Encrypt(key_, nonce, base::as_byte_span(token));

  // Return a concatenation of the nonce (counter) and the encrypted data, both
  // hex-encoded.
  return base::ToLowerASCII(base::HexEncode(nonce) +
                            base::HexEncode(ciphertext));
}

std::string CryptohomeTokenEncryptor::DecryptWithSystemSalt(
    std::string_view encrypted_token_hex) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return std::string(encrypted_token_hex);
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

  auto nonce = base::as_byte_span(encrypted_token).first<kNonceSize>();
  auto payload = base::as_byte_span(encrypted_token).subspan<kNonceSize>();

  return std::string(
      base::as_string_view(crypto::aes_ctr::Decrypt(key_, nonce, payload)));
}

std::string CryptohomeTokenEncryptor::WeakEncryptWithSystemSalt(
    const std::string& token) {
  // Only tests should ever use this.
  CHECK_IS_TEST();

  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return token;
  }

  return base::ToLowerASCII(base::HexEncode(
      crypto::aes_ctr::Encrypt(key_, nonce_, base::as_byte_span(token))));
}

std::string CryptohomeTokenEncryptor::WeakDecryptWithSystemSalt(
    const std::string& encrypted_token_hex) {
  // Don't care about token encryption while debugging.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return encrypted_token_hex;
  }

  std::vector<uint8_t> encrypted_token;
  if (!base::HexStringToBytes(encrypted_token_hex, &encrypted_token)) {
    LOG(WARNING) << "Corrupt encrypted token found.";
    return std::string();
  }
  return std::string(base::as_string_view(
      crypto::aes_ctr::Decrypt(key_, nonce_, encrypted_token)));
}

}  // namespace ash
