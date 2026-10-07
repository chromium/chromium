// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_linux.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_list.h"
#include "url/gurl.h"

namespace {

using proxy_resolver::mojom::LinuxProxyStatus;

constexpr char kProxyResolverInterface[] =
    "org.freedesktop.portal.ProxyResolver";
constexpr char kLookupMethod[] = "Lookup";

// Converts a proxy URI returned by the portal to a ProxyChain. The
// org.freedesktop.portal.ProxyResolver spec says proxy URIs have the form
// "protocol://[user[:password]@]host:port", or "direct://" when no proxy is
// needed. Anything else is rejected rather than leniently parsed, so that
// URIs accepted here are also understood by other consumers of the same
// configuration. Returns an invalid ProxyChain if `uri` is malformed or uses
// an unsupported protocol.
net::ProxyChain PortalUriToProxyChain(std::string_view uri) {
  if (uri == "direct://") {
    return net::ProxyChain::Direct();
  }

  // Reject whitespace, control characters (including NUL) and non-ASCII.
  if (!std::ranges::all_of(
          uri, [](char c) { return base::IsAsciiPrintable(c) && c != ' '; })) {
    return net::ProxyChain();
  }

  const size_t scheme_end = uri.find("://");
  if (scheme_end == std::string_view::npos || scheme_end == 0) {
    return net::ProxyChain();
  }
  const std::string_view scheme = uri.substr(0, scheme_end);
  if (base::EqualsCaseInsensitiveASCII(scheme, "direct")) {
    // "direct://" was handled above; it can't have an authority.
    return net::ProxyChain();
  }
  std::string_view authority = uri.substr(scheme_end + 3);

  // Drop the optional "user[:password]@". ProxyServer doesn't support
  // credentials; the user will be prompted for proxy authentication if the
  // proxy requires it.
  if (const size_t at = authority.find('@'); at != std::string_view::npos) {
    const std::string_view userinfo = authority.substr(0, at);
    if (userinfo.empty() || userinfo.front() == ':' ||
        std::ranges::count(userinfo, ':') > 1) {
      return net::ProxyChain();
    }
    authority = authority.substr(at + 1);
  }

  // What remains must be just the host and port, with no path, query,
  // fragment or further userinfo.
  if (authority.find_first_of("@/?#") != std::string_view::npos) {
    return net::ProxyChain();
  }

  // ProxyUriToProxyChain() validates the protocol, host and port. Note that it
  // interprets "socks://" as SOCKS5, which matches GLib's semantics.
  return net::ProxyUriToProxyChain(base::StrCat({scheme, "://", authority}),
                                   net::ProxyServer::SCHEME_HTTP,
                                   /*is_quic_allowed=*/false);
}

void RunCallback(
    const net::ProxyList& proxy_list,
    LinuxProxyStatus linux_status,
    ChromeMojoProxyResolverLinux::GetProxyForUrlCallback callback) {
  auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
  status->is_success = linux_status == LinuxProxyStatus::kOk;
  // `os_error` carries Windows and macOS error codes. The portal has no
  // numeric error code; failures are described by `linux_proxy_status`.
  status->os_error = 0;
  status->linux_proxy_status = linux_status;
  std::move(callback).Run(proxy_list, std::move(status));
}

}  // namespace

ChromeMojoProxyResolverLinux::ChromeMojoProxyResolverLinux(
    scoped_refptr<dbus::Bus> bus)
    : bus_(std::move(bus)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(bus_);
}

ChromeMojoProxyResolverLinux::~ChromeMojoProxyResolverLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
ChromeMojoProxyResolverLinux::CreateWithSelfOwnedReceiver() {
  mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver> remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<ChromeMojoProxyResolverLinux>(
                                  dbus_thread_linux::GetSharedSessionBus()),
                              remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void ChromeMojoProxyResolverLinux::GetProxyForUrl(
    const GURL& url,
    GetProxyForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (portal_state_) {
    case PortalState::kUninitialized:
      portal_state_ = PortalState::kInitializing;
      pending_requests_.emplace_back(url, std::move(callback));
      // This may run OnRequestXdgDesktopPortalComplete() synchronously if the
      // portal state is already known.
      dbus_xdg::RequestXdgDesktopPortal(
          bus_.get(), kProxyResolverInterface,
          base::BindOnce(
              &ChromeMojoProxyResolverLinux::OnRequestXdgDesktopPortalComplete,
              weak_ptr_factory_.GetWeakPtr()));
      return;
    case PortalState::kInitializing:
      pending_requests_.emplace_back(url, std::move(callback));
      return;
    case PortalState::kAvailable:
      Lookup(url, std::move(callback));
      return;
    case PortalState::kUnavailable:
      RunCallback(net::ProxyList(), LinuxProxyStatus::kPortalUnavailable,
                  std::move(callback));
      return;
  }
  NOTREACHED();
}

void ChromeMojoProxyResolverLinux::OnRequestXdgDesktopPortalComplete(
    uint32_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(portal_state_, PortalState::kInitializing);

  if (version == 0) {
    VLOG(1) << kProxyResolverInterface
            << " is unavailable; system proxy resolution will fall back to "
               "DIRECT.";
    portal_state_ = PortalState::kUnavailable;
  } else {
    portal_state_ = PortalState::kAvailable;
    portal_proxy_ =
        bus_->GetObjectProxy(dbus_xdg::kPortalServiceName,
                             dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
  }

  auto pending_requests = std::move(pending_requests_);
  pending_requests_.clear();
  for (auto& [url, callback] : pending_requests) {
    GetProxyForUrl(url, std::move(callback));
  }
}

void ChromeMojoProxyResolverLinux::Lookup(const GURL& url,
                                          GetProxyForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(portal_proxy_);
  dbus_utils::CallMethod<"s", "as">(
      portal_proxy_, kProxyResolverInterface, kLookupMethod,
      base::BindOnce(&ChromeMojoProxyResolverLinux::OnLookupResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      url.spec());
}

void ChromeMojoProxyResolverLinux::OnLookupResponse(
    GetProxyForUrlCallback callback,
    dbus_utils::CallMethodResultSig<"as"> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    VLOG(1) << kProxyResolverInterface << "." << kLookupMethod
            << " failed: " << result.error().error_name << ": "
            << result.error().error_message;
    RunCallback(net::ProxyList(), LinuxProxyStatus::kDBusError,
                std::move(callback));
    return;
  }

  const std::vector<std::string>& uris = std::get<0>(result.value());
  if (uris.empty()) {
    RunCallback(net::ProxyList(), LinuxProxyStatus::kEmptyProxyList,
                std::move(callback));
    return;
  }

  net::ProxyList proxy_list;
  for (const std::string& uri : uris) {
    net::ProxyChain chain = PortalUriToProxyChain(uri);
    if (chain.IsValid()) {
      proxy_list.AddProxyChain(chain);
    } else {
      VLOG(1) << "Ignoring invalid or unsupported proxy URI from portal: "
              << uri;
    }
  }

  RunCallback(proxy_list,
              proxy_list.IsEmpty() ? LinuxProxyStatus::kInvalidResponse
                                   : LinuxProxyStatus::kOk,
              std::move(callback));
}
