// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/prefetch/prefetch_proxy/chrome_speculation_host_delegate.h"

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_tab_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

ChromeSpeculationHostDelegate::ChromeSpeculationHostDelegate(
    content::RenderFrameHost& render_frame_host)
    : render_frame_host_(render_frame_host) {}

ChromeSpeculationHostDelegate::~ChromeSpeculationHostDelegate() = default;

void ChromeSpeculationHostDelegate::ProcessCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host_);
  auto* prefetch_proxy_tab_helper =
      PrefetchProxyTabHelper::FromWebContents(web_contents);

  // TODO(ryansturm): Handle non-private speculations.
  // https://crbug.com/1190331
  if (!prefetch_proxy_tab_helper)
    return;

  std::vector<GURL> private_prefetches;
  std::vector<GURL> private_prefetches_with_subresources;

  const url::Origin origin = render_frame_host_.GetLastCommittedOrigin();

  // Returns true if the given entry is processed. Being processed means this
  // delegate has a corresponding strategy to process the candidate, so it
  // extracts the candidate's URL.
  auto should_process_entry =
      [&](const blink::mojom::SpeculationCandidatePtr& candidate) {
        bool private_prefetch =
            candidate->requires_anonymous_client_ip_when_cross_origin &&
            !origin.IsSameOriginWith(url::Origin::Create(candidate->url));
        if (!private_prefetch)
          return false;
        if (candidate->action ==
            blink::mojom::SpeculationAction::kPrefetchWithSubresources) {
          private_prefetches_with_subresources.push_back(candidate->url);
          return true;
        }
        if (candidate->action == blink::mojom::SpeculationAction::kPrefetch) {
          private_prefetches.push_back(candidate->url);
          return true;
        }
        return false;
      };

  // Remove the entries that are to be processed by this delegate.
  auto new_end = std::remove_if(candidates.begin(), candidates.end(),
                                should_process_entry);
  candidates.erase(new_end, candidates.end());

  // TODO(ryansturm): Handle CSP prefetch-src. https://crbug.com/1192857
  if (private_prefetches.size() ||
      private_prefetches_with_subresources.size()) {
    prefetch_proxy_tab_helper->PrefetchSpeculationCandidates(
        private_prefetches_with_subresources, private_prefetches,
        render_frame_host_.GetLastCommittedURL());
  }
}
