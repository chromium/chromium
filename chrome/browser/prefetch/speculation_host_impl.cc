// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/speculation_host_impl.h"

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

// static
void SpeculationHostImpl::Bind(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesPrefetchProxy)) {
    mojo::ReportBadMessage(
        "Speculation rules must be enabled to bind to "
        "blink.mojom.SpeculationHost in the browser.");
    return;
  }

  // FrameServiceBase will destroy this on pipe closure or frame destruction.
  new SpeculationHostImpl(frame_host, std::move(receiver));
}

SpeculationHostImpl::SpeculationHostImpl(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver)
    : FrameServiceBase(frame_host, std::move(receiver)) {}

SpeculationHostImpl::~SpeculationHostImpl() = default;

void SpeculationHostImpl::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Only one update per document is allowed for now.
  // TODO(ryansturm): Add support for updates. https://crbug.com/1190338
  if (received_update_)
    return;
  received_update_ = true;

  // Only handle main frame current frame messages.
  if (!render_frame_host()->IsCurrent())
    return;
  if (render_frame_host()->GetParent())
    return;

  // TODO(ryansturm): Handle non-private speculations.
  // https://crbug.com/1190331
  std::vector<GURL> private_prefetches_with_subresources;
  std::vector<GURL> private_prefetches;

  for (auto& candidate : candidates) {
    if (candidate->url.HostIsIPAddress())
      continue;
    if (!candidate->url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    if (candidate->requires_anonymous_client_ip_when_cross_origin &&
        url::Origin::Create(candidate->url) != origin()) {
      if (candidate->action ==
          blink::mojom::SpeculationAction::kPrefetchWithSubresources) {
        private_prefetches_with_subresources.push_back(candidate->url);
      } else if (candidate->action ==
                 blink::mojom::SpeculationAction::kPrefetch) {
        private_prefetches.push_back(candidate->url);
      }
    }
  }

  if (private_prefetches.size() == 0 &&
      private_prefetches_with_subresources.size() == 0)
    return;
  auto* prefetch_proxy_tab_helper = PrefetchProxyTabHelper::FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host()));
  if (!prefetch_proxy_tab_helper)
    return;

  // TODO(ryansturm): Handle CSP prefetch-src. https://crbug.com/1192857
  prefetch_proxy_tab_helper->PrefetchSpeculationCandidates(
      private_prefetches_with_subresources, private_prefetches);
}
