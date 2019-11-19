// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PROXY_LOOKUP_CLIENT_IMPL_H_
#define CHROME_BROWSER_PREDICTORS_PROXY_LOOKUP_CLIENT_IMPL_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"

class GURL;

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace predictors {

using ProxyLookupCallback = base::OnceCallback<void(bool success)>;

// This class helps perform the proxy lookup using the NetworkContext.
// An instance of this class must be deleted after the callback is invoked.
class ProxyLookupClientImpl : public network::mojom::ProxyLookupClient {
 public:
  // Starts the proxy lookup for |url|. |callback| is called when the proxy
  // lookup is completed or when an error occurs.
  ProxyLookupClientImpl(const GURL& url,
                        ProxyLookupCallback callback,
                        network::mojom::NetworkContext* network_context);
  // Cancels the request if it hasn't been completed yet.
  ~ProxyLookupClientImpl() override;

  // network::mojom::ProxyLookupClient:
  void OnProxyLookupComplete(
      int32_t net_error,
      const base::Optional<net::ProxyInfo>& proxy_info) override;

 private:
  mojo::Receiver<network::mojom::ProxyLookupClient> receiver_{this};
  ProxyLookupCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ProxyLookupClientImpl);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PROXY_LOOKUP_CLIENT_IMPL_H_
