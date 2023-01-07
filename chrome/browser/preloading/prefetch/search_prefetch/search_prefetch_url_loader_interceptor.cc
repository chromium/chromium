// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader_interceptor.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"

SearchPrefetchURLLoaderInterceptor::SearchPrefetchURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

SearchPrefetchURLLoaderInterceptor::~SearchPrefetchURLLoaderInterceptor() =
    default;

// static
std::unique_ptr<SearchPrefetchURLLoader>
SearchPrefetchURLLoaderInterceptor::MaybeCreateLoaderForRequest(
    const network::ResourceRequest& tentative_resource_request,
    int frame_tree_node_id) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  // Make sure this is for a navigation.
  if (!web_contents) {
    return nullptr;
  }

  // Only intercept primary main frame and prerender main frame requests.
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  bool is_primary_main_frame_navigation =
      main_frame && main_frame->GetFrameTreeNodeId() == frame_tree_node_id;

  // Use the is_outermost_main_frame flag instead of obtaining the
  // corresponding RenderFrameHost via the `frame_tree_node_id` and checking
  // whether it has no parent frame, since the multipage architecture allows a
  // RenderFrameHost to be attached to another node, and we should avoid
  // relying on this dependency.
  bool can_activate_for_prerender =
      prerender_utils::IsSearchSuggestionPrerenderEnabled() &&
      prerender_utils::SearchPrefetchUpgradeToPrerenderIsEnabled() &&
      tentative_resource_request.is_outermost_main_frame &&
      web_contents->IsPrerenderedFrame(frame_tree_node_id);

  // This is not a primary navigation, nor can prerender use the prefetched
  // response.
  if (!is_primary_main_frame_navigation && !can_activate_for_prerender) {
    return nullptr;
  }

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return nullptr;

  SearchPrefetchService* service =
      SearchPrefetchServiceFactory::GetForProfile(profile);
  if (!service)
    return nullptr;

  if (can_activate_for_prerender) {
    return service->TakePrerenderFromMemoryCache(tentative_resource_request);
  }

  auto loader =
      service->TakePrefetchResponseFromMemoryCache(tentative_resource_request);
  if (loader)
    return loader;
  if (tentative_resource_request.load_flags & net::LOAD_SKIP_CACHE_VALIDATION) {
    return service->TakePrefetchResponseFromDiskCache(
        tentative_resource_request.url);
  }
  return nullptr;
}

void SearchPrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<SearchPrefetchURLLoader> prefetch =
      MaybeCreateLoaderForRequest(tentative_resource_request,
                                  frame_tree_node_id_);
  if (!prefetch) {
    std::move(callback).Run({});
    return;
  }

  auto* raw_prefetch = prefetch.get();

  // Hand ownership of the loader to the callback, when the callback runs,
  // mojo connection termination will manage it. If the callback is deleted,
  // the loader will be deleted.
  std::move(callback).Run(
      raw_prefetch->ServingResponseHandler(std::move(prefetch)));
}
