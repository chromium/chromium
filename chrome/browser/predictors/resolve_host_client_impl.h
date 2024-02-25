// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOLVE_HOST_CLIENT_IMPL_H_
#define CHROME_BROWSER_PREDICTORS_RESOLVE_HOST_CLIENT_IMPL_H_

#include <optional>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"

class GURL;

namespace net {
class NetworkAnonymizationKey;
}  // namespace net

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace predictors {

using ResolveHostCallback = base::OnceCallback<void(bool success)>;

// This class helps perform the host resolution using the NetworkContext.
// An instance of this class must be deleted after the callback is invoked.
class ResolveHostClientImpl : public network::ResolveHostClientBase {
 public:
  // Starts the host resolution for |url|. |callback| is called when the host is
  // resolved or when an error occurs.
  ResolveHostClientImpl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      ResolveHostCallback callback,
      network::mojom::NetworkContext* network_context);

  ResolveHostClientImpl(const ResolveHostClientImpl&) = delete;
  ResolveHostClientImpl& operator=(const ResolveHostClientImpl&) = delete;

  // Cancels the request if it hasn't been completed yet.
  ~ResolveHostClientImpl() override;

  // network::mojom::ResolveHostClient:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  void OnConnectionError();

 private:
  base::TimeTicks resolve_host_start_time_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  ResolveHostCallback callback_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOLVE_HOST_CLIENT_IMPL_H_
