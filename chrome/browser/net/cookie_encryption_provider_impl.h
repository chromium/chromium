// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_COOKIE_ENCRYPTION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_NET_COOKIE_ENCRYPTION_PROVIDER_IMPL_H_

#include "base/callback_list.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cookie_encryption_provider.mojom.h"

// Implementation of CookieEncryptionProvider interface. This is Windows only
// for now, but will be expanded to other platforms in future.
class CookieEncryptionProviderImpl
    : public network::mojom::CookieEncryptionProvider {
 public:
  CookieEncryptionProviderImpl();
  ~CookieEncryptionProviderImpl() override;

  CookieEncryptionProviderImpl(const CookieEncryptionProviderImpl&) = delete;
  CookieEncryptionProviderImpl& operator=(const CookieEncryptionProviderImpl&) =
      delete;

  // mojom::CookieEncryptionProvider implementation.
  void GetEncryptor(GetEncryptorCallback callback) override;

  // Returns a mojo::PendingRemote to this instance. Adds a receiver to
  // `receivers_`.
  mojo::PendingRemote<network::mojom::CookieEncryptionProvider> BindNewRemote();

 private:
  std::list<base::CallbackListSubscription> subscriptions_;
  mojo::ReceiverSet<network::mojom::CookieEncryptionProvider> receivers_;
};

#endif  // CHROME_BROWSER_NET_COOKIE_ENCRYPTION_PROVIDER_IMPL_H_
