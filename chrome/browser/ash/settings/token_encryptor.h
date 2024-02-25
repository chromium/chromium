// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_TOKEN_ENCRYPTOR_H_
#define CHROME_BROWSER_ASH_SETTINGS_TOKEN_ENCRYPTOR_H_

#include <memory>
#include <string>
#include <string_view>

namespace crypto {
class SymmetricKey;
}

namespace ash {

// Interface class for classes that encrypt and decrypt tokens using the
// system salt.
class TokenEncryptor {
 public:
  virtual ~TokenEncryptor() {}

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
  // Converts |passphrase| to a SymmetricKey using the given |salt|.
  std::unique_ptr<crypto::SymmetricKey> PassphraseToKey(
      const std::string& passphrase,
      const std::string& salt);

  // The cached system salt passed to the constructor, originally coming
  // from cryptohome daemon.
  std::string system_salt_;

  // A key based on the system salt.  Useful for encrypting device-level
  // data for which we have no additional credentials.
  std::unique_ptr<crypto::SymmetricKey> system_salt_key_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_TOKEN_ENCRYPTOR_H_
