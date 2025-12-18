// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROXYING_URL_LOADER_FACTORY_H_

#include "base/check_is_test.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace enterprise_auth {

class PlatformAuthProxyingURLLoaderFactoryTest;

// This class intercepts outgoing url requests, if the request should be handled
// by a credential sso extension it is executed using URLSessionURLLoader.
// Otherwise, the request is propagated unmodified down the chain.

// Lifetime of this object is self-managed. MaybeProxyRequest constructs
// the instance on the heap and once there receiver set is empty or the target
// factory has disconnected, the instance destroys itself.
class ProxyingURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  ~ProxyingURLLoaderFactory() override;
  ProxyingURLLoaderFactory(const ProxyingURLLoaderFactory&) = delete;
  ProxyingURLLoaderFactory& operator=(const ProxyingURLLoaderFactory&) = delete;

  static void MaybeProxyRequest(
      const url::Origin& request_initiator,
      ChromeContentBrowserClient::URLLoaderFactoryType type,
      network::URLLoaderFactoryBuilder& factory_builder);

  // Checks if request matches pattern of Okta's SSO URL request, which is:
  // POST
  // https://<DOMAIN>/idp/idx/authenticators/sso_extension/transactions/<ID>/verify
  static bool IsOktaSSORequest(const network::ResourceRequest& request);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

 private:
  ProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory);

  void OnTargetFactoryDisconnect();

  void OnProxyDisconnect();

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;

  inline void SetDestructionCallbackForTesting(
      base::OnceCallback<void()> callback) {
    CHECK_IS_TEST();
    destruction_callback_for_testing_ = std::move(callback);
  }

  // Setting this will cause the callback to be called instead of
  // URLSessionURLLoader::CreateAndStart().
  inline void SetRequestInterceptedCallbackForTesting(
      base::OnceCallback<void(const network::ResourceRequest&)> callback) {
    CHECK_IS_TEST();
    intercepted_request_callback_for_testing_ = std::move(callback);
  }

  friend class PlatformAuthProxyingURLLoaderFactoryTest;

  // Only for testing purposes.
  base::OnceCallback<void()> destruction_callback_for_testing_;
  base::OnceCallback<void(const network::ResourceRequest&)>
      intercepted_request_callback_for_testing_;
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROXYING_URL_LOADER_FACTORY_H_
