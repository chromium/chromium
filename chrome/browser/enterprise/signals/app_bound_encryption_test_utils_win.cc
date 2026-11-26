// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/app_bound_encryption_test_utils_win.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "components/os_crypt/async/common/algorithm.mojom.h"

namespace device_signals::test {

ScopedAppBoundEncryptionOverrides::ScopedAppBoundEncryptionOverrides() {
  os_crypt::SetOverridesForTesting(this);  // IN-TEST
}

ScopedAppBoundEncryptionOverrides::~ScopedAppBoundEncryptionOverrides() {
  os_crypt::SetOverridesForTesting(nullptr);  // IN-TEST
}

HRESULT ScopedAppBoundEncryptionOverrides::EncryptAppBoundString(
    ProtectionLevel level,
    const std::string& plaintext,
    std::string& ciphertext,
    DWORD& last_error,
    elevation_service::EncryptFlags* flags) {
  return E_NOTIMPL;
}

HRESULT ScopedAppBoundEncryptionOverrides::DecryptAppBoundString(
    const std::string& ciphertext,
    std::string& plaintext,
    ProtectionLevel protection_level,
    std::optional<std::string>& new_ciphertext,
    DWORD& last_error,
    elevation_service::EncryptFlags* flags) {
  return E_NOTIMPL;
}

os_crypt::SupportLevel
ScopedAppBoundEncryptionOverrides::GetAppBoundEncryptionSupportLevel(
    PrefService* local_state) {
  ++support_level_call_count_;
  return support_level_;
}

FakeAppBoundKeyProvider::FakeAppBoundKeyProvider(std::string tag,
                                                 bool use_for_encryption)
    : tag_(std::move(tag)), use_for_encryption_(use_for_encryption) {}

FakeAppBoundKeyProvider::~FakeAppBoundKeyProvider() = default;

void FakeAppBoundKeyProvider::GetKey(KeyCallback callback) {
  std::move(callback).Run(tag_, CreateFakeAppBoundKey());
}

bool FakeAppBoundKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

DeferredFakeAppBoundKeyProvider::DeferredFakeAppBoundKeyProvider(
    bool use_for_encryption,
    base::OnceCallback<void(KeyCallback)> on_get_key)
    : use_for_encryption_(use_for_encryption),
      on_get_key_(std::move(on_get_key)) {}

DeferredFakeAppBoundKeyProvider::~DeferredFakeAppBoundKeyProvider() = default;

void DeferredFakeAppBoundKeyProvider::GetKey(KeyCallback callback) {
  std::move(on_get_key_).Run(std::move(callback));
}

bool DeferredFakeAppBoundKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

os_crypt_async::Encryptor::Key CreateFakeAppBoundKey() {
  const std::vector<uint8_t> key_bytes(
      os_crypt_async::Encryptor::Key::kAES256GCMKeySize, 0x42);
  return os_crypt_async::Encryptor::Key(
      key_bytes, os_crypt_async::mojom::Algorithm::kAES256GCM);
}

std::unique_ptr<os_crypt_async::OSCryptAsync> CreateFakeOSCryptAsync(
    std::string tag,
    bool use_for_encryption) {
  std::vector<std::pair<os_crypt_async::OSCryptAsync::Precedence,
                        std::unique_ptr<os_crypt_async::KeyProvider>>>
      providers;
  providers.emplace_back(/*precedence=*/10u,
                         std::make_unique<FakeAppBoundKeyProvider>(
                             std::move(tag), use_for_encryption));
  return std::make_unique<os_crypt_async::OSCryptAsync>(std::move(providers));
}

}  // namespace device_signals::test
