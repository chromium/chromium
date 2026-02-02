// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/encryption/cache_encryption_provider_impl.h"

#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/policy/core/common/policy_logger.h"
#include "crypto/random.h"

namespace enterprise_encryption {

namespace {
constexpr size_t kPrimaryKeySizeInBytes = 32;  // AES-256
}

CacheEncryptionProviderImpl::CacheEncryptionProviderImpl(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    std::vector<uint8_t> encrypted_primary_key,
    StoreKeyCallback store_key_callback)
    : os_crypt_async_(os_crypt_async),
      encrypted_primary_key_(std::move(encrypted_primary_key)),
      store_key_callback_(store_key_callback) {}

CacheEncryptionProviderImpl::~CacheEncryptionProviderImpl() = default;

void CacheEncryptionProviderImpl::GetEncryptor(GetEncryptorCallback callback) {
  os_crypt_async_->GetInstance(std::move(callback));
}

void CacheEncryptionProviderImpl::GetEncryptedCacheEncryptionKey(
    GetEncryptedCacheEncryptionKeyCallback callback) {
  os_crypt_async_->GetInstance(
      base::BindOnce(&CacheEncryptionProviderImpl::OnEncryptorReadyForKey,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CacheEncryptionProviderImpl::OnEncryptorReadyForKey(
    GetEncryptedCacheEncryptionKeyCallback callback,
    os_crypt_async::Encryptor encryptor) {
  bool needs_new_key = true;

  if (!encrypted_primary_key_.empty()) {
    // Validate the key by trying to decrypt it
    std::optional<std::string> decrypted =
        encryptor.DecryptData(encrypted_primary_key_);
    if (decrypted && decrypted->length() == kPrimaryKeySizeInBytes) {
      DVLOG(1) << "Successfully validated existing encrypted cache encryption key.";
      needs_new_key = false;
    } else {
      // TODO: crbug.com/475800166 - Log errors in UMA.
      LOG(ERROR) << "Failed to decrypt/validate existing cache encryption key.";
      LOG_POLICY(ERROR, POLICY_PROCESSING)
          << "Failed to decrypt/validate existing cache encryption key.";
      // TODO: crbug.com/476958143 - Check flags returned by os_crypt, implement
      // retries if needed.
      encrypted_primary_key_.clear();
    }
  } else {
    DVLOG(1) << "No existing cache encryption key found in prefs. Generating "
                "a new one.";
  }

  if (needs_new_key) {
    DVLOG(1) << "Generating new cache encryption key.";
    std::vector<uint8_t> primary_key_bytes(kPrimaryKeySizeInBytes);
    crypto::RandBytes(primary_key_bytes);

    std::optional<std::vector<uint8_t>> encrypted = encryptor.EncryptString(
        std::string(primary_key_bytes.begin(), primary_key_bytes.end()));

    if (encrypted) {
      encrypted_primary_key_ = *encrypted;
      store_key_callback_.Run(encrypted_primary_key_);
      DVLOG(1) << "Successfully generated and stored new encrypted cache "
                  "encryption key.";
    } else {
      LOG(ERROR) << "Failed to encrypt new cache encryption key. "
                    "Cache encryption will be disabled for this session.";
      encrypted_primary_key_.clear();
    }
  }

  // In case key obtaining failed, return an empty key. It will result in
  // cache initialization failure and thus cache will be disabled.
  // TODO: crbug.com/475800166 - Log errors in UMA.
  std::move(callback).Run(encrypted_primary_key_);
}

mojo::PendingRemote<
    network::mojom::CacheEncryptionProvider>
CacheEncryptionProviderImpl::BindNewRemote() {
  mojo::PendingRemote<
      network::mojom::CacheEncryptionProvider>
      pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace enterprise_encryption
