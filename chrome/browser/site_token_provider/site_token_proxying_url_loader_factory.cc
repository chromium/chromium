// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_proxying_url_loader_factory.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/browser/browser_context.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"

namespace site_token_provider {

SiteTokenProxyingURLLoaderFactory::SiteTokenProxyingURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    CheckUrlCallback check_url_callback,
    base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key),
      check_url_callback_(std::move(check_url_callback)) {
  CHECK(base::FeatureList::IsEnabled(features::kSiteTokenProviderEnabled));
  CHECK(target_factory_remote.is_valid());
  CHECK(check_url_callback_);
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(
      base::BindOnce(&SiteTokenProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));
}

SiteTokenProxyingURLLoaderFactory::~SiteTokenProxyingURLLoaderFactory() =
    default;

// static
void SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(
    content::BrowserContext* browser_context,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!base::FeatureList::IsEnabled(features::kSiteTokenProviderEnabled)) {
    return;
  }

  Profile* profile =
      browser_context ? Profile::FromBrowserContext(browser_context) : nullptr;
  auto* service = profile
                      ? SiteTokenProviderServiceFactory::GetForProfile(profile)
                      : nullptr;
  if (!service) {
    return;
  }

  auto check_url_callback = base::BindRepeating(
      [](base::WeakPtr<SiteTokenProviderService> weak_service,
         const GURL& url) {
        if (!weak_service || !url.is_valid() || !url.has_host()) {
          return false;
        }
        if (!url.SchemeIsCryptographic() && !net::IsLocalhost(url)) {
          return false;
        }
        if (!weak_service->IsDomainAllowlisted(url.host())) {
          return false;
        }
        return !weak_service->GetTokenForDomain(url.host()).empty();
      },
      service->GetWeakPtr());

  auto [receiver, remote] = factory_builder.Append();
  base::MakeSelfDeleting<SiteTokenProxyingURLLoaderFactory>(
      std::move(receiver), std::move(remote), std::move(check_url_callback));
}

void SiteTokenProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  uint32_t new_options = options;

  if (check_url_callback_.Run(request.url)) {
    new_options |= network::mojom::kURLLoadOptionUseHeaderClient;
  }

  target_factory_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                        new_options, request, std::move(client),
                                        traffic_annotation);
}

void SiteTokenProxyingURLLoaderFactory::OnTargetFactoryError() {
  target_factory_.reset();
  DisconnectReceiversAndDestroy();
}

}  // namespace site_token_provider
