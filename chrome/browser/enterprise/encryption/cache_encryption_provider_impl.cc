// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/encryption/cache_encryption_provider_impl.h"

#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "crypto/random.h"

namespace enterprise_encryption {

namespace {
constexpr size_t kMasterKeySizeInBytes = 32;  // AES-256
}

CacheEncryptionProviderImpl::CacheEncryptionProviderImpl(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    std::vector<uint8_t> encrypted_master_key,
    StoreKeyCallback store_key_callback)
    : os_crypt_async_(os_crypt_async),
      encrypted_master_key_(std::move(encrypted_master_key)),
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

  if (!encrypted_master_key_.empty()) {
    // Validate the key by trying to decrypt it
    std::optional<std::string> decrypted =
        encryptor.DecryptData(encrypted_master_key_);
    if (decrypted && decrypted->length() == kMasterKeySizeInBytes) {
      DVLOG(1) << "Successfully validated existing encrypted cache encryption key.";
      needs_new_key = false;
    } else {
      LOG(ERROR) << "Failed to decrypt/validate existing cache encryption key, "
                    "or key size mismatch. Generating a new one.";
      encrypted_master_key_.clear();
    }
  } else {
    DVLOG(1)
        << "No existing cache encryption key found in prefs. Generating a new one.";
  }

  if (needs_new_key) {
    DVLOG(1) << "Generating new cache encryption key.";
    std::vector<uint8_t> master_key_bytes(kMasterKeySizeInBytes);
    crypto::RandBytes(master_key_bytes);

    std::optional<std::vector<uint8_t>> encrypted = encryptor.EncryptString(
        std::string(master_key_bytes.begin(), master_key_bytes.end()));

    if (encrypted) {
      encrypted_master_key_ = *encrypted;
      store_key_callback_.Run(encrypted_master_key_);
      DVLOG(1) << "Successfully generated and stored new encrypted cache "
                  "encryption key.";
    } else {
      LOG(ERROR) << "Failed to encrypt new cache encryption key. "
                    "Cache encryption will be disabled for this session.";
      encrypted_master_key_.clear();
    }
  }

  // In case key obtaining failed, return an empty key. It will result in
  // cache initialization failure and thus cache will be disabled.
  // TODO: crbug.com/475800166 - Log errors in UMA.
  std::move(callback).Run(encrypted_master_key_);
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