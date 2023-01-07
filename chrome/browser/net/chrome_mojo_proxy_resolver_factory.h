// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_
#define CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

// ProxyResolverFactory that acts as a proxy to the proxy resolver service.
// Starts the service as needed, and maintains no active mojo pipes to it,
// so that it's automatically shut down as needed.
//
// ChromeMojoProxyResolverFactories must be created and used only on the UI
// thread.
class ChromeMojoProxyResolverFactory
    : public proxy_resolver::mojom::ProxyResolverFactory {
 public:
  ChromeMojoProxyResolverFactory();

  ChromeMojoProxyResolverFactory(const ChromeMojoProxyResolverFactory&) =
      delete;
  ChromeMojoProxyResolverFactory& operator=(
      const ChromeMojoProxyResolverFactory&) = delete;

  ~ChromeMojoProxyResolverFactory() override;

  // Convenience method that creates a self-owned ProxyResolverFactory and
  // returns a remote endpoint to control it.
  static mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
  CreateWithSelfOwnedReceiver();

  // proxy_resolver::mojom::ProxyResolverFactory implementation:
  void CreateResolver(
      const std::string& pac_script,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
      mojo::PendingRemote<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client)
      override;

 private:
  std::unique_ptr<service_manager::Connector> service_manager_connector_;
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_FACTORY_H_
