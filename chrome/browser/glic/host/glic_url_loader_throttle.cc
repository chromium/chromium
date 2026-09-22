// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_url_loader_throttle.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_cors_exempt_headers.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace glic {

namespace {

bool IsGlicHeaderInjectionEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicHeader) &&
         features::IsGlicNoWebviewEnabled();
}

bool ShouldInjectGlicHeaders(content::WebContents* web_contents,
                             content::FrameTreeNodeId frame_tree_node_id) {
  if (!IsGlicHeaderInjectionEnabled()) {
    return false;
  }
  if (!web_contents || !IsGlicGuest(web_contents)) {
    return false;
  }
  if (frame_tree_node_id && web_contents->GetPrimaryMainFrame() &&
      web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId() !=
          frame_tree_node_id) {
    return false;
  }
  return true;
}

bool ShouldInjectGlicHeaders(content::RenderFrameHost* frame) {
  return frame && ShouldInjectGlicHeaders(
                      content::WebContents::FromRenderFrameHost(frame),
                      frame->GetFrameTreeNodeId());
}

}  // namespace

// static
std::unique_ptr<GlicURLLoaderThrottle> GlicURLLoaderThrottle::MaybeCreate(
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& request) {
  content::WebContents* web_contents = wc_getter ? wc_getter.Run() : nullptr;
  if (!ShouldInjectGlicHeaders(web_contents, frame_tree_node_id)) {
    return nullptr;
  }

  return std::make_unique<GlicURLLoaderThrottle>();
}

// static
void GlicURLLoaderThrottle::SetHeaders(net::HttpRequestHeaders* headers) {
  headers->SetHeader(kGlicHeaderName, kGlicHeaderValue);
  headers->SetHeader(kGlicVersionHeaderName, version_info::GetVersionNumber());
  headers->SetHeader(kGlicChannelHeaderName,
                     version_info::GetChannelString(chrome::GetChannel()));
}

GlicURLLoaderThrottle::GlicURLLoaderThrottle() = default;

GlicURLLoaderThrottle::~GlicURLLoaderThrottle() = default;

void GlicURLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                             bool* defer) {
  SetHeaders(&request->cors_exempt_headers);
}

void GlicURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  SetHeaders(&headers_update_params->modified_cors_exempt_headers);
}

GlicSubresourceProxyingURLLoaderFactory::
    GlicSubresourceProxyingURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            target_factory_remote,
        base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key) {
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &GlicSubresourceProxyingURLLoaderFactory::OnTargetFactoryError,
      base::Unretained(this)));
}

GlicSubresourceProxyingURLLoaderFactory::
    ~GlicSubresourceProxyingURLLoaderFactory() = default;

// static
void GlicSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
    content::RenderFrameHost* frame,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!ShouldInjectGlicHeaders(frame)) {
    return;
  }
  auto [receiver, remote] = factory_builder.Append();
  base::MakeSelfDeleting<GlicSubresourceProxyingURLLoaderFactory>(
      std::move(receiver), std::move(remote));
}

void GlicSubresourceProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::ResourceRequest modified_request = request;
  GlicURLLoaderThrottle::SetHeaders(&modified_request.cors_exempt_headers);

  target_factory_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                        options, modified_request,
                                        std::move(client), traffic_annotation);
}

void GlicSubresourceProxyingURLLoaderFactory::OnTargetFactoryError() {
  DisconnectReceiversAndDestroy();
}

}  // namespace glic
