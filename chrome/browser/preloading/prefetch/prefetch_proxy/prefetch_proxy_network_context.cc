// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_network_context.h"

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_network_context_client.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxy_configurator.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"
#include "net/http/http_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

PrefetchProxyNetworkContext::PrefetchProxyNetworkContext(Profile* profile,
                                                         bool is_isolated,
                                                         bool use_proxy)
    : profile_(profile), is_isolated_(is_isolated), use_proxy_(use_proxy) {}

PrefetchProxyNetworkContext::~PrefetchProxyNetworkContext() = default;

network::mojom::NetworkContext* PrefetchProxyNetworkContext::GetNetworkContext()
    const {
  DCHECK(network_context_);
  return network_context_.get();
}

network::mojom::URLLoaderFactory*
PrefetchProxyNetworkContext::GetUrlLoaderFactory() {
  if (!url_loader_factory_) {
    if (is_isolated_) {
      CreateIsolatedUrlLoaderFactory();
      DCHECK(network_context_);
    } else {
      // TODO(crbug.com/1278103): Use
      // RenderFrameHost::CreateNetworkServiceDefaultFactory if possible.
      url_loader_factory_ = profile_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
    }
  }
  DCHECK(url_loader_factory_);
  return url_loader_factory_.get();
}

network::mojom::CookieManager* PrefetchProxyNetworkContext::GetCookieManager() {
  DCHECK(is_isolated_);
  DCHECK(network_context_);
  if (!cookie_manager_)
    network_context_->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());

  return cookie_manager_.get();
}

void PrefetchProxyNetworkContext::CreateNewUrlLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    absl::optional<net::IsolationInfo> isolation_info) {
  // Since PrefetchProxy code doesn't handle same origin prefetches with
  // subresources, we don't have to worry about this function being called when
  // network_context_ is not bound.
  DCHECK(is_isolated_);
  DCHECK(network_context_);

  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::mojom::kBrowserProcessId;
  factory_params->is_trusted = true;
  factory_params->is_corb_enabled = false;
  if (isolation_info) {
    factory_params->isolation_info = *isolation_info;
  }

  GetNetworkContext()->CreateURLLoaderFactory(std::move(pending_receiver),
                                              std::move(factory_params));
}

void PrefetchProxyNetworkContext::CloseIdleConnections() {
  if (network_context_)
    network_context_->CloseIdleConnections(base::DoNothing());
}

void PrefetchProxyNetworkContext::CreateIsolatedUrlLoaderFactory() {
  DCHECK(is_isolated_);

  network_context_.reset();
  url_loader_factory_.reset();

  PrefetchProxyService* prefetch_proxy_service =
      PrefetchProxyServiceFactory::GetForProfile(profile_);

  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->user_agent = content::GetReducedUserAgent(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent),
      version_info::GetMajorVersionNumber());
  context_params->accept_language = net::HttpUtil::GenerateAcceptLanguageHeader(
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));
  context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->cors_exempt_header_list = {
      content::kCorsExemptPurposeHeaderName};
  context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();

  context_params->http_cache_enabled = true;
  DCHECK(!context_params->http_cache_directory);

  if (use_proxy_) {
    context_params->initial_custom_proxy_config =
        prefetch_proxy_service->proxy_configurator()->CreateCustomProxyConfig();
    context_params->custom_proxy_connection_observer_remote =
        prefetch_proxy_service->proxy_configurator()
            ->NewProxyConnectionObserverRemote();

    // Register a client config receiver so that updates to the set of proxy
    // hosts or proxy headers will be updated.
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client;
    context_params->custom_proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
    prefetch_proxy_service->proxy_configurator()->AddCustomProxyConfigClient(
        std::move(config_client), base::DoNothing());
  }

  // Explicitly disallow network service features which could cause a privacy
  // leak.
  context_params->enable_certificate_reporting = false;
  context_params->enable_domain_reliability = false;

  content::CreateNetworkContextInNetworkService(
      network_context_.BindNewPipeAndPassReceiver(), std::move(context_params));

  if (use_proxy_) {
    // Configure a context client to ensure Web Reports and other privacy leak
    // surfaces won't be enabled.
    mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<PrefetchProxyNetworkContextClient>(),
        client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(client_remote));
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory_remote;

  CreateNewUrlLoaderFactory(
      isolated_factory_remote.InitWithNewPipeAndPassReceiver(), absl::nullopt);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
          std::move(isolated_factory_remote)));
}
