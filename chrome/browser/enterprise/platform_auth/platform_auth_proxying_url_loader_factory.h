// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_PROXYING_URL_LOADER_FACTORY_H_

#include "base/check_is_test.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/origin.h"

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

  // Injects this factory into the factory_builder if all apply:
  //    - PlatformAuthProviderManager is enabled
  //    - `type` is kDocumentSubresource
  //    - `request_initiator.scheme()` is HTTPS
  //    - prefs::kExtensibleEnterpriseSSOEnabledIdps contains
  //      kOktaIdentityProvider
  //    - prefs::kExtensibleEnterpriseSSOConfiguredHosts contains
  //      `request_initiator.host()`
  static void MaybeProxyRequest(
      const url::Origin& request_initiator,
      ChromeContentBrowserClient::URLLoaderFactoryType type,
      content::BrowserContext* context,
      network::URLLoaderFactoryBuilder& factory_builder);

  // While alive all requests executed by
  // URLSessionURLLoader will return 200 OK with
  // |URLSessionURLLoader::kTestServerResponseBody|.
  // There should every only be one instance of this object.
  class ScopedURLSessionOverrideForTesting {
   public:
    ScopedURLSessionOverrideForTesting();
    ~ScopedURLSessionOverrideForTesting();

   private:
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test_;
    static bool instance_exists_;
  };

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
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      base::flat_set<std::string> configured_hosts,
      const url::Origin& request_initiator);

  void OnTargetFactoryDisconnect();

  void OnProxyDisconnect();

  bool ShouldInterceptRequest(const network::ResourceRequest& request);

  base::flat_set<std::string> configured_hosts_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  const url::Origin request_initiator_;

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
