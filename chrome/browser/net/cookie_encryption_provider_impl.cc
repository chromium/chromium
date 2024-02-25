// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cookie_encryption_provider_impl.h"

#include "chrome/browser/browser_process.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"

CookieEncryptionProviderImpl::CookieEncryptionProviderImpl() = default;

CookieEncryptionProviderImpl::~CookieEncryptionProviderImpl() = default;

void CookieEncryptionProviderImpl::GetEncryptor(GetEncryptorCallback callback) {
  subscriptions_.push_back(
      g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
          [](GetEncryptorCallback callback, os_crypt_async::Encryptor encryptor,
             bool result) { std::move(callback).Run(std::move(encryptor)); },
          std::move(callback))));
}

mojo::PendingRemote<network::mojom::CookieEncryptionProvider>
CookieEncryptionProviderImpl::BindNewRemote() {
  mojo::PendingRemote<network::mojom::CookieEncryptionProvider> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}
