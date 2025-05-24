// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_IDENTITY_CHROMEOS_TOKEN_ENCRYPTOR_H_
#define CHROME_BROWSER_DEVICE_IDENTITY_CHROMEOS_TOKEN_ENCRYPTOR_H_

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace ash {

// Interface class for classes that encrypt and decrypt tokens using the
// system salt.
//
// This class supports two methods of encryption: the old "weak" method and the
// new method. Unfortunately neither method provides actual confidentiality
// protection and both have completely equivalent strength. Both methods rely on
// the system salt, which is stored in the clear on disk. The old method is:
//
//   AES-256-CTR(key = salt, counter = salt[0 .. 15], token)
//
// and the new method is:
//
//   counter = random_bytes(16)
//   counter || AES-256-CTR(key = salt, counter = counter, token)
//
// so in both cases there are no actual secrets involved as either key or
// counter. The only practical distinction is that the new method generates
// larger encoded values and that two separate encryptions of the same token
// will yield different encoded values.
class TokenEncryptor {
 public:
  virtual ~TokenEncryptor() = default;

  // Encrypts |token| with the system salt key (stable for the lifetime
  // of the device).  Useful to avoid storing plain text in place like
  // Local State.
  virtual std::string EncryptWithSystemSalt(std::string_view token) = 0;

  // Decrypts |token| with the system salt key (stable for the lifetime
  // of the device).
  virtual std::string DecryptWithSystemSalt(
      std::string_view encrypted_token_hex) = 0;

  // Old deprecated versions of Encrypt and Decrypt. These functions are weak
  // because they do not use a proper counter with the encryptor.
  //
  // The WeakEncrypt function will CHECK-fail if called in non-test code. No new
  // code should ever use it, the function is only kept to enable testing of
  // WeakDecrypt. The WeakDecrypt is available to allow code to read old tokens.
  virtual std::string WeakEncryptWithSystemSalt(const std::string& token) = 0;
  virtual std::string WeakDecryptWithSystemSalt(
      const std::string& encrypted_token_hex) = 0;
};

// TokenEncryptor based on the system salt from cryptohome daemon. This
// implementation is used in production.
class CryptohomeTokenEncryptor : public TokenEncryptor {
 public:
  explicit CryptohomeTokenEncryptor(const std::string& system_salt);

  CryptohomeTokenEncryptor(const CryptohomeTokenEncryptor&) = delete;
  CryptohomeTokenEncryptor& operator=(const CryptohomeTokenEncryptor&) = delete;

  ~CryptohomeTokenEncryptor() override;

  // TokenEncryptor overrides:
  std::string EncryptWithSystemSalt(std::string_view token) override;
  std::string DecryptWithSystemSalt(
      std::string_view encrypted_token_hex) override;
  std::string WeakEncryptWithSystemSalt(const std::string& token) override;
  std::string WeakDecryptWithSystemSalt(
      const std::string& encrypted_token_hex) override;

 private:
  static constexpr size_t kDerivedKeySize = 32;
  static constexpr size_t kNonceSize = 16;

  std::array<uint8_t, kDerivedKeySize> key_;
  std::array<uint8_t, kNonceSize> nonce_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_DEVICE_IDENTITY_CHROMEOS_TOKEN_ENCRYPTOR_H_
