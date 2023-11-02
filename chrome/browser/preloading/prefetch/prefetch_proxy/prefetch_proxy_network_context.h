// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_NETWORK_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

// An isolated network context used for prefetches. The purpose of using a
// separate network context is to set a custom proxy configuration, and separate
// any cookies.
class PrefetchProxyNetworkContext {
 public:
  PrefetchProxyNetworkContext(Profile* profile,
                              bool is_isolated,
                              bool use_proxy);
  ~PrefetchProxyNetworkContext();

  PrefetchProxyNetworkContext(const PrefetchProxyNetworkContext&) = delete;
  PrefetchProxyNetworkContext& operator=(const PrefetchProxyNetworkContext) =
      delete;

  // Get a reference to |network_context_|.
  network::mojom::NetworkContext* GetNetworkContext() const;

  // Get a reference to |url_loader_factory_|. If it is null, then
  // |network_context_| is bound and configured, and a new
  // |SharedURLLoaderFactory| is created.
  network::mojom::URLLoaderFactory* GetUrlLoaderFactory();

  // Get a reference to |cookie_manager_|. If it is null, then it is bound to
  // the cookie manager of |network_context_|.
  network::mojom::CookieManager* GetCookieManager();

  // Binds |pending_receiver| to a URL loader factory associated with
  // |network_context_|.
  void CreateNewUrlLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      absl::optional<net::IsolationInfo> isolation_info);

  // Close any idle connections with |network_context_|.
  void CloseIdleConnections();

  base::WeakPtr<PrefetchProxyNetworkContext> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Bind |network_context_| to a new network context and configure it to use
  // the prefetch proxy. Also set up |url_loader_factory_| as a new URL loader
  // factory for |network_context_|.
  void CreateIsolatedUrlLoaderFactory();

  // The profile to use when configuring |network_context_|.
  raw_ptr<Profile> profile_;

  // Whether an isolated network context should be used, or if the default
  // network context should be used.
  const bool is_isolated_;

  // Whether the network context should be configured to use the Prefetch Proxy.
  const bool use_proxy_;

  // The network context and URL loader factory to use when making prefetches.
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The cookie manager for the isolated |network_context_|. This is used when
  // copying cookies from the isolated prefetch network context to the default
  // network context after a prefetch is committed.
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  base::WeakPtrFactory<PrefetchProxyNetworkContext> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_NETWORK_CONTEXT_H_
