// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_win.h"

#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/strings/grit/services_strings.h"

namespace {

using proxy_resolver_win::mojom::WindowsSystemProxyResolver;

WindowsSystemProxyResolver* GetProxyResolver(
    const base::TimeDelta& idle_timeout) {
  static base::NoDestructor<mojo::Remote<WindowsSystemProxyResolver>> remote;
  if (!remote->is_bound()) {
    content::ServiceProcessHost::Launch(
        remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_WINDOWS_SYSTEM_PROXY_RESOLVER_DISPLAY_NAME)
            .Pass());

    // The service will report itself idle once there are no more bound
    // WindowsSystemProxyResolver instances. This will happen pretty frequently,
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

ChromeMojoProxyResolverWin::ChromeMojoProxyResolverWin(
    const base::TimeDelta& idle_timeout)
    : idle_timeout_(idle_timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ChromeMojoProxyResolverWin::~ChromeMojoProxyResolverWin() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

mojo::PendingRemote<WindowsSystemProxyResolver>
ChromeMojoProxyResolverWin::CreateWithSelfOwnedReceiver() {
  mojo::PendingRemote<WindowsSystemProxyResolver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ChromeMojoProxyResolverWin>(base::Minutes(5)),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<WindowsSystemProxyResolver>
ChromeMojoProxyResolverWin::CreateWithSelfOwnedReceiverForTesting(
    const base::TimeDelta& idle_timeout) {
  mojo::PendingRemote<WindowsSystemProxyResolver> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ChromeMojoProxyResolverWin>(idle_timeout),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void ChromeMojoProxyResolverWin::GetProxyForUrl(
    const GURL& url,
    GetProxyForUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetProxyResolver(idle_timeout_)->GetProxyForUrl(url, std::move(callback));
}
