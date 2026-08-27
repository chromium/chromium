// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_url_loader_header_client.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_token_provider/site_token_header_client.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/resource_request.h"

namespace site_token_provider {

// static
void SiteTokenURLLoaderHeaderClient::Create(
    base::WeakPtr<SiteTokenProviderService> service,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        target_client) {
  DCHECK(base::FeatureList::IsEnabled(features::kSiteTokenProviderEnabled));
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new SiteTokenURLLoaderHeaderClient(
          std::move(service), std::move(target_client))),
      std::move(receiver));
}

// static
void SiteTokenURLLoaderHeaderClient::MaybeWrap(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client) {
  if (!header_client || !browser_context) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kSiteTokenProviderEnabled)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* service = SiteTokenProviderServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      original_header_client = std::move(*header_client);

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      new_header_client;
  SiteTokenURLLoaderHeaderClient::Create(
      service->GetWeakPtr(), new_header_client.InitWithNewPipeAndPassReceiver(),
      std::move(original_header_client));

  *header_client = std::move(new_header_client);
}

SiteTokenURLLoaderHeaderClient::SiteTokenURLLoaderHeaderClient(
    base::WeakPtr<SiteTokenProviderService> service,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        target_client)
    : service_(std::move(service)) {
  if (target_client) {
    target_client_.Bind(std::move(target_client));
    target_client_.set_disconnect_handler(
        base::BindOnce(&SiteTokenURLLoaderHeaderClient::OnTargetDisconnect,
                       base::Unretained(this)));
  }
}

SiteTokenURLLoaderHeaderClient::~SiteTokenURLLoaderHeaderClient() = default;

void SiteTokenURLLoaderHeaderClient::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_header_client;
  if (target_client_.is_bound()) {
    target_client_->OnLoaderCreated(
        request_id, target_header_client.InitWithNewPipeAndPassReceiver());
  }

  SiteTokenHeaderClient::Create(service_, std::move(receiver),
                                std::move(target_header_client));
}

void SiteTokenURLLoaderHeaderClient::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  if (target_client_.is_bound()) {
    target_client_->OnLoaderForCorsPreflightCreated(request,
                                                    std::move(receiver));
  }
}

void SiteTokenURLLoaderHeaderClient::OnTargetDisconnect() {
  target_client_.reset();
}

}  // namespace site_token_provider
