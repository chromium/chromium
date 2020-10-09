// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/url_loader_factory_proxy_impl.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

// static
void UrlLoaderFactoryProxyImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<chrome::mojom::UrlLoaderFactoryProxy> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<UrlLoaderFactoryProxyImpl>(frame_host),
      std::move(receiver));
}

UrlLoaderFactoryProxyImpl::UrlLoaderFactoryProxyImpl(
    content::RenderFrameHost* frame_host)
    : frame_id_(content::GlobalFrameRoutingId(frame_host->GetProcess()->GetID(),
                                              frame_host->GetRoutingID())) {}

UrlLoaderFactoryProxyImpl::~UrlLoaderFactoryProxyImpl() = default;

void UrlLoaderFactoryProxyImpl::GetProxiedURLLoaderFactory(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> proxied_factory) {
  auto* frame_host = content::RenderFrameHost::FromID(frame_id_);
  if (!frame_host)
    return;
  auto* process = frame_host->GetProcess();
  auto* browser_context = process->GetBrowserContext();
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  DCHECK(web_request_api);

  web_request_api->MaybeProxyURLLoaderFactory(
      browser_context, frame_host, process->GetID(),
      content::ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      /*navigation_id=*/base::nullopt,
      base::UkmSourceId::FromInt64(frame_host->GetPageUkmSourceId()),
      &proxied_factory,
      /*headber_client=*/nullptr);

  mojo::FusePipes(std::move(proxied_factory), std::move(original_factory));
}
