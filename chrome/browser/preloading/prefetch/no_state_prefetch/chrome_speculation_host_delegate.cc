// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_speculation_host_delegate.h"

#include <algorithm>
#include <vector>

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

ChromeSpeculationHostDelegate::ChromeSpeculationHostDelegate(
    content::RenderFrameHost& render_frame_host)
    : render_frame_host_(render_frame_host) {}

ChromeSpeculationHostDelegate::~ChromeSpeculationHostDelegate() {
  for (auto& prefetch : same_origin_no_state_prefetches_) {
    prefetch->OnNavigateAway();
  }
}

void ChromeSpeculationHostDelegate::ProcessCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&*render_frame_host_);

  // Same origin prefetches with subresources are handled by NSP.
  std::vector<GURL> same_origin_prefetches_with_subresources;

  const url::Origin& origin = render_frame_host_->GetLastCommittedOrigin();

  // Returns true if the given entry is processed. Being processed means this
  // delegate has a corresponding strategy to process the candidate, so it
  // extracts the candidate's URL.
  auto should_process_entry =
      [&](const blink::mojom::SpeculationCandidatePtr& candidate) {
        bool is_same_origin = origin.IsSameOriginWith(candidate->url);

        if (!is_same_origin) {
          return false;
        }

        if (candidate->action !=
            blink::mojom::SpeculationAction::kPrefetchWithSubresources) {
          return false;
        }

        same_origin_prefetches_with_subresources.push_back(candidate->url);
        return true;
      };

  // Remove the entries that are to be processed by this delegate.
  std::erase_if(candidates, should_process_entry);

  if (same_origin_prefetches_with_subresources.size() > 0) {
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            render_frame_host_->GetBrowserContext());
    if (!no_state_prefetch_manager) {
      return;
    }
    content::SessionStorageNamespace* session_storage_namespace =
        web_contents->GetController().GetDefaultSessionStorageNamespace();
    gfx::Size size = web_contents->GetContainerBounds().size();
    // The chrome implementation almost certainly only allows one NSP to start
    // (500 ms limit), but treat all requests of this class as handled by
    // chrome.
    for (const auto& url : same_origin_prefetches_with_subresources) {
      std::unique_ptr<prerender::NoStatePrefetchHandle> handle =
          no_state_prefetch_manager->AddSameOriginSpeculation(
              url, session_storage_namespace, size, origin);
      if (handle) {
        same_origin_no_state_prefetches_.push_back(std::move(handle));
      }
    }
  }
}
