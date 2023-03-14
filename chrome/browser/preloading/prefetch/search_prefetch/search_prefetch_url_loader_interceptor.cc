// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader_interceptor.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"

namespace {

SearchPrefetchService* GetSearchPrefetchService(int frame_tree_node_id) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return nullptr;
  }
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }
  return SearchPrefetchServiceFactory::GetForProfile(profile);
}

}  // namespace

SearchPrefetchURLLoaderInterceptor::SearchPrefetchURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

SearchPrefetchURLLoaderInterceptor::~SearchPrefetchURLLoaderInterceptor() =
    default;

// static
SearchPrefetchURLLoader::RequestHandler
SearchPrefetchURLLoaderInterceptor::MaybeCreateLoaderForRequest(
    const network::ResourceRequest& tentative_resource_request,
    int frame_tree_node_id) {
  // Do not intercept non-main frame navigations.
  if (!tentative_resource_request.is_outermost_main_frame) {
    // Use the is_outermost_main_frame flag instead of obtaining the
    // corresponding RenderFrameHost via the `frame_tree_node_id` and checking
    // whether it has no parent frame, since the multipage architecture allows a
    // RenderFrameHost to be attached to another node, and we should avoid
    // relying on this dependency.
    return {};
  }

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return {};
  }

  // Only intercept primary main frame and prerender main frame requests.
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  bool is_primary_main_frame_navigation =
      main_frame && main_frame->GetFrameTreeNodeId() == frame_tree_node_id;
  bool is_prerender_main_frame_navigation =
      web_contents->IsPrerenderedFrame(frame_tree_node_id);

  // Only intercepts primary and prerender main frame navigations.
  if (!is_primary_main_frame_navigation &&
      !is_prerender_main_frame_navigation) {
    return {};
  }

  SearchPrefetchService* service = GetSearchPrefetchService(frame_tree_node_id);
  if (!service) {
    return {};
  }

  if (is_prerender_main_frame_navigation) {
    // Note, if SearchPrerenderFallbackToPrefetchIsEnabled() is true, prerender
    // cannot take the prefetch response away, and it can only make a copy of
    // the response. In this case, TakePrerenderFromMemoryCache cannot be
    // called, and no URLLoader would be returned, so we stop at this point.
    if (!prerender_utils::IsSearchSuggestionPrerenderEnabled() ||
        !prerender_utils::SearchPrefetchUpgradeToPrerenderIsEnabled()) {
      return {};
    }
    if (prerender_utils::SearchPreloadShareableCacheIsEnabled()) {
      return service->MaybeCreateResponseReader(tentative_resource_request);
    }
    return service->TakePrerenderFromMemoryCache(tentative_resource_request);
  }

  DCHECK(is_primary_main_frame_navigation);
  auto handler =
      service->TakePrefetchResponseFromMemoryCache(tentative_resource_request);
  if (handler) {
    return handler;
  }
  if (tentative_resource_request.load_flags & net::LOAD_SKIP_CACHE_VALIDATION) {
    return service->TakePrefetchResponseFromDiskCache(
        tentative_resource_request.url);
  }
  return {};
}

void SearchPrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SearchPrefetchURLLoader::RequestHandler prefetched_loader_handler =
      MaybeCreateLoaderForRequest(tentative_resource_request,
                                  frame_tree_node_id_);

  std::move(callback).Run(std::move(prefetched_loader_handler));
}
