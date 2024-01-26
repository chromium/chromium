// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PROXY_LOOKUP_CLIENT_IMPL_H_
#define CHROME_BROWSER_PREDICTORS_PROXY_LOOKUP_CLIENT_IMPL_H_

#include <optional>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"

class GURL;

namespace net {
class NetworkAnonymizationKey;
}

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
  ProxyLookupClientImpl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      ProxyLookupCallback callback,
      network::mojom::NetworkContext* network_context);

  ProxyLookupClientImpl(const ProxyLookupClientImpl&) = delete;
  ProxyLookupClientImpl& operator=(const ProxyLookupClientImpl&) = delete;

  // Cancels the request if it hasn't been completed yet.
  ~ProxyLookupClientImpl() override;

  // network::mojom::ProxyLookupClient:
  void OnProxyLookupComplete(
      int32_t net_error,
      const std::optional<net::ProxyInfo>& proxy_info) override;

 private:
  base::TimeTicks proxy_lookup_start_time_;
  mojo::Receiver<network::mojom::ProxyLookupClient> receiver_{this};
  ProxyLookupCallback callback_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PROXY_LOOKUP_CLIENT_IMPL_H_
