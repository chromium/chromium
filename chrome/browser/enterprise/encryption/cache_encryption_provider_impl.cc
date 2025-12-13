// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/encryption/cache_encryption_provider_impl.h"

#include "components/os_crypt/async/browser/os_crypt_async.h"

namespace enterprise_encryption {

CacheEncryptionProviderImpl::CacheEncryptionProviderImpl(
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : os_crypt_async_(os_crypt_async) {}

CacheEncryptionProviderImpl::~CacheEncryptionProviderImpl() = default;

void CacheEncryptionProviderImpl::GetEncryptor(GetEncryptorCallback callback) {
  os_crypt_async_->GetInstance(std::move(callback));
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