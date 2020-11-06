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
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_from_string_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "content/public/browser/browser_context.h"
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
  url_ = tentative_resource_request.url;

  std::unique_ptr<PrefetchedResponseContainer> prefetch =
      GetPrefetchedResponse(url_);
  if (!prefetch) {
    DoNotInterceptPrefetchedNavigation();
    return;
  }

  InterceptPrefetchedNavigation(tentative_resource_request,
                                std::move(prefetch));
}

void SearchPrefetchURLLoaderInterceptor::InterceptPrefetchedNavigation(
    const network::ResourceRequest& tentative_resource_request,
    std::unique_ptr<PrefetchedResponseContainer> prefetch) {
  std::unique_ptr<SearchPrefetchFromStringURLLoader> url_loader =
      std::make_unique<SearchPrefetchFromStringURLLoader>(
          std::move(prefetch), tentative_resource_request);
  std::move(loader_callback_).Run(url_loader->ServingResponseHandler());
  // url_loader manages its own lifetime once bound to the mojo pipes.
  url_loader.release();
}

void SearchPrefetchURLLoaderInterceptor::DoNotInterceptPrefetchedNavigation() {
  std::move(loader_callback_).Run({});
}

std::unique_ptr<PrefetchedResponseContainer>
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
