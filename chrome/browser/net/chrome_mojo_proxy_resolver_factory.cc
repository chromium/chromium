// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/child_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"  // nogncheck crbug.com/1125897
#else
#include "content/public/browser/service_process_host.h"
#include "services/strings/grit/services_strings.h"
#endif

namespace {

proxy_resolver::mojom::ProxyResolverFactory* GetProxyResolverFactory() {
  static base::NoDestructor<
      mojo::Remote<proxy_resolver::mojom::ProxyResolverFactory>>
      remote;
  if (!remote->is_bound()) {
#if BUILDFLAG(IS_ANDROID)
    // For Android we just lazily initialize a single factory instance and keep
    // it around forever.
    static base::NoDestructor<proxy_resolver::ProxyResolverFactoryImpl> factory(
        remote->BindNewPipeAndPassReceiver());
#else
    // For other platforms we launch the resolver in its own sandboxed service
    // process.
    content::ServiceProcessHost::Launch(
        remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_PROXY_RESOLVER_DISPLAY_NAME)
            .Pass());

    // The service will report itself idle once there are no more bound
    // ProxyResolver instances. We drop the Remote at that point to initiate
    // service process termination. Any subsequent call to
    // |GetProxyResolverFactory()| will launch a new process.
    remote->reset_on_idle_timeout(base::TimeDelta());

    // Also reset on disconnection in case, e.g., the service crashes.
    remote->reset_on_disconnect();
#endif
  }

  return remote->get();
}

}  // namespace

ChromeMojoProxyResolverFactory::ChromeMojoProxyResolverFactory() = default;

ChromeMojoProxyResolverFactory::~ChromeMojoProxyResolverFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
ChromeMojoProxyResolverFactory::CreateWithSelfOwnedReceiver() {
  mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ChromeMojoProxyResolverFactory>(),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void ChromeMojoProxyResolverFactory::CreateResolver(
    const std::string& pac_script,
    mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
    mojo::PendingRemote<
        proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetProxyResolverFactory()->CreateResolver(pac_script, std::move(receiver),
                                            std::move(client));
}
