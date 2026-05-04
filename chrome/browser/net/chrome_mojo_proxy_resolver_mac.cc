// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_mac.h"

#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

using proxy_resolver::mojom::SystemProxyResolver;

SystemProxyResolver* GetProxyResolver(const base::TimeDelta& idle_timeout) {
  static base::NoDestructor<mojo::Remote<SystemProxyResolver>> remote;
  if (!remote->is_bound()) {
    content::ServiceProcessHost::Launch(
        remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            // TODO(crbug.com/442313607): Replace with a translatable string
            // resource from services_strings.grd.
            .WithDisplayName(u"macOS System Proxy Resolver")
            .Pass());

    // The service will report itself idle once there are no more bound
    // SystemProxyResolver instances. This will happen pretty frequently,
    // so we wait for |idle_timeout| before dropping the Remote to initiate
    // service process termination. Any subsequent call to GetProxyForUrl() will
    // launch a new process.
    remote->reset_on_idle_timeout(idle_timeout);

    // Also reset on disconnect in case, e.g., the service crashes.
    remote->reset_on_disconnect();
  }

  return remote->get();
}

}  // namespace

ChromeMojoProxyResolverMac::ChromeMojoProxyResolverMac(
    const base::TimeDelta& idle_timeout)
    : idle_timeout_(idle_timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ChromeMojoProxyResolverMac::~ChromeMojoProxyResolverMac() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

mojo::PendingRemote<SystemProxyResolver>
ChromeMojoProxyResolverMac::CreateWithSelfOwnedReceiver() {
  mojo::PendingRemote<SystemProxyResolver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ChromeMojoProxyResolverMac>(base::Minutes(5)),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<SystemProxyResolver>
ChromeMojoProxyResolverMac::CreateWithSelfOwnedReceiverForTesting(  // IN-TEST
    const base::TimeDelta& idle_timeout) {
  mojo::PendingRemote<SystemProxyResolver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ChromeMojoProxyResolverMac>(idle_timeout),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void ChromeMojoProxyResolverMac::GetProxyForUrl(
    const GURL& url,
    GetProxyForUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetProxyResolver(idle_timeout_)->GetProxyForUrl(url, std::move(callback));
}
