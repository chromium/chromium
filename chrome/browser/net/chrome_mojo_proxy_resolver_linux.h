// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_LINUX_H_
#define CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_LINUX_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/dbus/utils/call_method.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#include "url/gurl.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

// Implements proxy_resolver::mojom::SystemProxyResolver in the browser process
// using the org.freedesktop.portal.ProxyResolver XDG desktop portal. The
// network service calls into this when --use-system-proxy-resolver is passed.
//
// The portal applies the host's proxy configuration (including PAC scripts and
// WPAD) on the host side, so this works even when Chrome is sandboxed (e.g. in
// Flatpak) and cannot read the desktop's proxy settings directly.
//
// All methods must be called on the UI thread, since D-Bus calls on the shared
// session bus must be made from there.
class ChromeMojoProxyResolverLinux
    : public proxy_resolver::mojom::SystemProxyResolver {
 public:
  // `bus` is the D-Bus connection to use.
  explicit ChromeMojoProxyResolverLinux(scoped_refptr<dbus::Bus> bus);
  ChromeMojoProxyResolverLinux(const ChromeMojoProxyResolverLinux&) = delete;
  ChromeMojoProxyResolverLinux& operator=(const ChromeMojoProxyResolverLinux&) =
      delete;
  ~ChromeMojoProxyResolverLinux() override;

  // Convenience method that creates a self-owned
  // proxy_resolver::mojom::SystemProxyResolver that uses the shared D-Bus
  // session bus, and returns a remote pointing to it.
  static mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
  CreateWithSelfOwnedReceiver();

  // proxy_resolver::mojom::SystemProxyResolver:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override;

 private:
  enum class PortalState {
    kUninitialized,
    kInitializing,
    kAvailable,
    kUnavailable,
  };

  void OnRequestXdgDesktopPortalComplete(uint32_t version);
  void Lookup(const GURL& url, GetProxyForUrlCallback callback);
  void OnLookupResponse(GetProxyForUrlCallback callback,
                        dbus_utils::CallMethodResultSig<"as"> result);

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> portal_proxy_ = nullptr;
  PortalState portal_state_ = PortalState::kUninitialized;

  // Requests received while the portal is initializing.
  std::vector<std::pair<GURL, GetProxyForUrlCallback>> pending_requests_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ChromeMojoProxyResolverLinux> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NET_CHROME_MOJO_PROXY_RESOLVER_LINUX_H_
