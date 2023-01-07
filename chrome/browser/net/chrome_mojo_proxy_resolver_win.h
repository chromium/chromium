// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_WIN_H_
#define CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_WIN_H_

#include "base/time/time.h"
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"
#include "url/gurl.h"

// WindowsSystemProxyResolver that acts as a proxy to the proxy_resolver_win
// service. Starts the service as needed, and maintains no active mojo pipes to
// it, so that it's automatically shut down as needed.
//
// ChromeMojoProxyResolverWin must be created and used only on the UI thread.
class ChromeMojoProxyResolverWin
    : public proxy_resolver_win::mojom::WindowsSystemProxyResolver {
 public:
  explicit ChromeMojoProxyResolverWin(const base::TimeDelta& idle_timeout);
  ChromeMojoProxyResolverWin(const ChromeMojoProxyResolverWin&) = delete;
  ChromeMojoProxyResolverWin& operator=(const ChromeMojoProxyResolverWin&) =
      delete;
  ~ChromeMojoProxyResolverWin() override;

  // Convenience method that creates a self-owned WindowsSystemProxyResolver and
  // returns a remote endpoint to control it.
  static mojo::PendingRemote<WindowsSystemProxyResolver>
  CreateWithSelfOwnedReceiver();

  // The test version allows for setting a custom idle timeout.
  static mojo::PendingRemote<WindowsSystemProxyResolver>
  CreateWithSelfOwnedReceiverForTesting(const base::TimeDelta& idle_timeout);

  // proxy_resolver_win::mojom::WindowsSystemProxyResolver implementation:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override;

 private:
  const base::TimeDelta idle_timeout_;
};

#endif  // CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_WIN_H_
