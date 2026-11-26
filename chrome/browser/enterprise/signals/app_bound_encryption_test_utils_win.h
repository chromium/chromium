// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_APP_BOUND_ENCRYPTION_TEST_UTILS_WIN_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_APP_BOUND_ENCRYPTION_TEST_UTILS_WIN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace device_signals::test {

// RAII test override for `os_crypt::AppBoundEncryptionOverridesForTesting` that
// stubs out elevation service COM calls and lets tests configure the reported
// `os_crypt::SupportLevel`.
class ScopedAppBoundEncryptionOverrides
    : public os_crypt::AppBoundEncryptionOverridesForTesting {
 public:
  ScopedAppBoundEncryptionOverrides();
  ~ScopedAppBoundEncryptionOverrides() override;

  ScopedAppBoundEncryptionOverrides(const ScopedAppBoundEncryptionOverrides&) =
      delete;
  ScopedAppBoundEncryptionOverrides& operator=(
      const ScopedAppBoundEncryptionOverrides&) = delete;

  void set_support_level(os_crypt::SupportLevel support_level) {
    support_level_ = support_level;
  }

  int support_level_call_count() const { return support_level_call_count_; }

  // os_crypt::AppBoundEncryptionOverridesForTesting:
  HRESULT EncryptAppBoundString(
      ProtectionLevel level,
      const std::string& plaintext,
      std::string& ciphertext,
      DWORD& last_error,
      elevation_service::EncryptFlags* flags) override;
  HRESULT DecryptAppBoundString(
      const std::string& ciphertext,
      std::string& plaintext,
      ProtectionLevel protection_level,
      std::optional<std::string>& new_ciphertext,
      DWORD& last_error,
      elevation_service::EncryptFlags* flags) override;
  os_crypt::SupportLevel GetAppBoundEncryptionSupportLevel(
      PrefService* local_state) override;

 private:
  os_crypt::SupportLevel support_level_ = os_crypt::SupportLevel::kSupported;
  int support_level_call_count_ = 0;
};

// Fake `os_crypt_async::KeyProvider` that synchronously resolves `GetKey()`
// with a test AES-256-GCM key under `tag`.
class FakeAppBoundKeyProvider : public os_crypt_async::KeyProvider {
 public:
  explicit FakeAppBoundKeyProvider(std::string tag,
                                   bool use_for_encryption = true);
  ~FakeAppBoundKeyProvider() override;

 private:
  // os_crypt_async::KeyProvider:
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;

  std::string tag_;
  bool use_for_encryption_;
};

// Fake `os_crypt_async::KeyProvider` that forwards `GetKey()`'s `KeyCallback`
// to `on_get_key` so tests can verify asynchronous completion before resolving
// the key.
class DeferredFakeAppBoundKeyProvider : public os_crypt_async::KeyProvider {
 public:
  DeferredFakeAppBoundKeyProvider(
      bool use_for_encryption,
      base::OnceCallback<void(KeyCallback)> on_get_key);
  ~DeferredFakeAppBoundKeyProvider() override;

 private:
  // os_crypt_async::KeyProvider:
  void GetKey(KeyCallback callback) override;
  bool UseForEncryption() override;

  bool use_for_encryption_;
  base::OnceCallback<void(KeyCallback)> on_get_key_;
};

// Creates a dummy AES-256-GCM `Encryptor::Key` suitable for resolving a
// `KeyProvider::KeyCallback` in tests.
os_crypt_async::Encryptor::Key CreateFakeAppBoundKey();

// Creates an `OSCryptAsync` instance backed by a single
// `FakeAppBoundKeyProvider`.
std::unique_ptr<os_crypt_async::OSCryptAsync> CreateFakeOSCryptAsync(
    std::string tag,
    bool use_for_encryption = true);

}  // namespace device_signals::test

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_APP_BOUND_ENCRYPTION_TEST_UTILS_WIN_H_
