// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader_interceptor.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/prefetch/search_prefetch/prefetched_response_container.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace {

Profile* ProfileFromFrameTreeNodeID(int frame_tree_node_id) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

}  // namespace

SearchPrefetchURLLoaderInterceptor::SearchPrefetchURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

SearchPrefetchURLLoaderInterceptor::~SearchPrefetchURLLoaderInterceptor() =
    default;

void SearchPrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!loader_callback_);
  loader_callback_ = std::move(callback);

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  // Make sure this is for a navigation.
  if (!web_contents) {
    DoNotInterceptPrefetchedNavigation();
    return;
  }
  // Only intercept main frame requests.
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  if (!main_frame || main_frame->GetFrameTreeNodeId() != frame_tree_node_id_) {
    DoNotInterceptPrefetchedNavigation();
    return;
  }

  url_ = tentative_resource_request.url;

  std::unique_ptr<SearchPrefetchURLLoader> prefetch =
      GetPrefetchedResponse(url_);
  if (!prefetch) {
    DoNotInterceptPrefetchedNavigation();
    return;
  }

  InterceptPrefetchedNavigation(std::move(prefetch));
}

void SearchPrefetchURLLoaderInterceptor::InterceptPrefetchedNavigation(
    std::unique_ptr<SearchPrefetchURLLoader> prefetch) {
  std::move(loader_callback_).Run(prefetch->ServingResponseHandler());
  // url_loader manages its own lifetime once bound to the mojo pipes.
  prefetch.release();
}

void SearchPrefetchURLLoaderInterceptor::DoNotInterceptPrefetchedNavigation() {
  std::move(loader_callback_).Run({});
}

std::unique_ptr<SearchPrefetchURLLoader>
SearchPrefetchURLLoaderInterceptor::GetPrefetchedResponse(const GURL& url) {
  auto* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id_);
  if (!profile)
    return nullptr;

  SearchPrefetchService* service =
      SearchPrefetchServiceFactory::GetForProfile(profile);
  if (!service)
    return nullptr;

  return service->TakePrefetchResponse(url);
}
