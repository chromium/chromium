// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_ENCRYPTION_CACHE_ENCRYPTION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_ENCRYPTION_CACHE_ENCRYPTION_PROVIDER_IMPL_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cache_encryption_provider.mojom.h"

namespace os_crypt_async {
class OSCryptAsync;
}

namespace enterprise_encryption {

// Implementation of CacheEncryptionProvider interface.
class CacheEncryptionProviderImpl
    : public network::mojom::CacheEncryptionProvider {
 public:
  using StoreKeyCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>&)>;

  explicit CacheEncryptionProviderImpl(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      std::vector<uint8_t> encrypted_primary_key,
      StoreKeyCallback store_key_callback);
  ~CacheEncryptionProviderImpl() override;

  CacheEncryptionProviderImpl(const CacheEncryptionProviderImpl&) = delete;
  CacheEncryptionProviderImpl& operator=(const CacheEncryptionProviderImpl&) =
      delete;

  // mojom::CacheEncryptionProvider implementation.
  void GetEncryptor(GetEncryptorCallback callback) override;

  // Returns the encrypted cache encryption key from the profile preferences.
  // Create one if it doesn't exist.
  void GetEncryptedCacheEncryptionKey(
      GetEncryptedCacheEncryptionKeyCallback callback) override;

  // Returns a mojo::PendingRemote to this instance.
  mojo::PendingRemote<
      network::mojom::CacheEncryptionProvider>
  BindNewRemote();

 private:
  void OnEncryptorReadyForKey(GetEncryptedCacheEncryptionKeyCallback callback,
                              os_crypt_async::Encryptor encryptor);

  mojo::ReceiverSet<network::mojom::CacheEncryptionProvider>
      receivers_;
  raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::vector<uint8_t> encrypted_primary_key_;
  StoreKeyCallback store_key_callback_;
  base::WeakPtrFactory<CacheEncryptionProviderImpl> weak_ptr_factory_{this};
};

}  // namespace enterprise_encryption

#endif  // CHROME_BROWSER_ENTERPRISE_ENCRYPTION_CACHE_ENCRYPTION_PROVIDER_IMPL_H_
