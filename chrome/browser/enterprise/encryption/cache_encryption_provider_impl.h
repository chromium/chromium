// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_ENCRYPTION_CACHE_ENCRYPTION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_ENCRYPTION_CACHE_ENCRYPTION_PROVIDER_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/mojom/cache_encryption_provider.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace os_crypt_async {
class OSCryptAsync;
}

namespace enterprise_encryption {

// Implementation of CacheEncryptionProvider interface.
class CacheEncryptionProviderImpl : public network::mojom::CacheEncryptionProvider {
 public:
  explicit CacheEncryptionProviderImpl(
      os_crypt_async::OSCryptAsync* os_crypt_async);
  ~CacheEncryptionProviderImpl() override;

  CacheEncryptionProviderImpl(const CacheEncryptionProviderImpl&) = delete;
  CacheEncryptionProviderImpl& operator=(const CacheEncryptionProviderImpl&) =
      delete;

  // mojom::CacheEncryptionProvider implementation.
  void GetEncryptor(GetEncryptorCallback callback) override;

  // Returns a mojo::PendingRemote to this instance.
  mojo::PendingRemote<
      network::mojom::CacheEncryptionProvider>
  BindNewRemote();

 private:
  mojo::ReceiverSet<
      network::mojom::CacheEncryptionProvider>
      receivers_;
  raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

}  // namespace enterprise_encryption

#endif  // CHROME_BROWSER_ENTERPRISE_ENCRYPTION_CACHE_ENCRYPTION_PROVIDER_IMPL_H_
