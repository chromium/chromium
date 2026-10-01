// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_NETWORK_REQUEST_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_NETWORK_REQUEST_PROXYING_URL_LOADER_FACTORY_H_

#include <cstdint>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "components/enterprise/connectors/core/common.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace enterprise_connectors {

// Proxies the URLLoaderFactory used by a frame for navigations or subresources
// to scan the body of POST requests according to the
// "OnNetworkRequestEnterpriseConnector" policy.
//
// Only audit-only policies are supported, so requests are always forwarded to
// the target factory right away and scanned in parallel for reporting
// purposes. Scans are owned independently of this factory so that they can
// complete even if the document that made the request goes away.
//
// Limitations:
// - Only the initial URL of a request is checked against the policy, so a
//   request isn't scanned again if it's redirected.
// - Streaming request bodies (e.g. a `ReadableStream` passed to `fetch()`)
//   aren't scanned since they can only be read once, by the network request.
class NetworkRequestProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  using ScanCompletedCallback =
      base::RepeatingCallback<void(const RequestHandlerResult&)>;

  NetworkRequestProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      content::GlobalRenderFrameHostId frame_id,
      base::SelfDeletingPassKey pass_key);
  NetworkRequestProxyingURLLoaderFactory(
      const NetworkRequestProxyingURLLoaderFactory&) = delete;
  NetworkRequestProxyingURLLoaderFactory& operator=(
      const NetworkRequestProxyingURLLoaderFactory&) = delete;

  // Sets a callback that is run with the result of every scan started by this
  // class once it completes.
  [[nodiscard]] static base::AutoReset<ScanCompletedCallback>
  SetScanCompletedCallbackForTesting(ScanCompletedCallback callback);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

 private:
  ~NetworkRequestProxyingURLLoaderFactory() override;

  // Starts scanning the body of `request` if the policy applies to it.
  void MaybeStartScan(const network::ResourceRequest& request);

  void OnTargetFactoryError();

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;

  // The frame the proxied factory was created for.
  const content::GlobalRenderFrameHostId frame_id_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_NETWORK_REQUEST_PROXYING_URL_LOADER_FACTORY_H_
