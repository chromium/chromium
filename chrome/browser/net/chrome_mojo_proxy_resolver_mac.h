// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_MAC_H_
#define CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_MAC_H_

#include "base/time/time.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "url/gurl.h"

// MacSystemProxyResolver that acts as a proxy to the proxy_resolver_mac
// service. Starts the service as needed, and maintains no active mojo pipes to
// it, so that it's automatically shut down as needed.
//
// ChromeMojoProxyResolverMac must be created and used only on the UI thread.
class ChromeMojoProxyResolverMac
    : public proxy_resolver::mojom::SystemProxyResolver {
 public:
  explicit ChromeMojoProxyResolverMac(const base::TimeDelta& idle_timeout);
  ChromeMojoProxyResolverMac(const ChromeMojoProxyResolverMac&) = delete;
  ChromeMojoProxyResolverMac& operator=(const ChromeMojoProxyResolverMac&) =
      delete;
  ~ChromeMojoProxyResolverMac() override;

  // Convenience method that creates a self-owned MacSystemProxyResolver and
  // returns a remote endpoint to control it.
  static mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
  CreateWithSelfOwnedReceiver();

  // The test version allows for setting a custom idle timeout.
  static mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
  CreateWithSelfOwnedReceiverForTesting(const base::TimeDelta& idle_timeout);

  // proxy_resolver::mojom::SystemProxyResolver implementation:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override;

 private:
  const base::TimeDelta idle_timeout_;
};

#endif  // CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_MAC_H_
