// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader_interceptor.h"

#include <cstddef>
#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

SearchPrefetchService* GetSearchPrefetchService(
    content::FrameTreeNodeId frame_tree_node_id) {
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
  // If the feature is enabled, ensure SearchPrefetchService so that the
  // navigation can consult the search prefetch cache regardless of if
  // SearchPrefetchService has been accessed before this line, for example,
  // during browser startup.
  if (base::FeatureList::IsEnabled(kEnsureSearchPrefetchServiceOnInterceptor)) {
    return SearchPrefetchServiceFactory::GetForProfile(profile);
  }
  return SearchPrefetchServiceFactory::GetForProfileIfExists(profile);
}

void SearchPrefetchRequestHandler(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client) {
  url_loader_factory->CreateLoaderAndStart(
      std::move(url_loader), 0, 0, resource_request,
      std::move(url_loader_client), net::MutableNetworkTrafficAnnotationTag());
}

}  // namespace

SearchPrefetchURLLoaderInterceptor::SearchPrefetchURLLoaderInterceptor(
    content::FrameTreeNodeId frame_tree_node_id,
    int64_t navigation_id,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
    : frame_tree_node_id_(frame_tree_node_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  navigation_id_ = navigation_id;
  navigation_response_task_runner_ = navigation_response_task_runner;
#else
  std::ignore = navigation_id;
  std::ignore = navigation_response_task_runner;
#endif
}

SearchPrefetchURLLoaderInterceptor::~SearchPrefetchURLLoaderInterceptor() =
    default;

// static
SearchPrefetchURLLoader::RequestHandler
SearchPrefetchURLLoaderInterceptor::MaybeCreateLoaderForRequest(
    const network::ResourceRequest& tentative_resource_request,
    content::FrameTreeNodeId frame_tree_node_id) {
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
    if (!prerender_utils::IsSearchSuggestionPrerenderEnabled()) {
      return {};
    }
    return service->MaybeCreateResponseReader(tentative_resource_request);
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

SearchPrefetchURLLoader::RequestHandler
SearchPrefetchURLLoaderInterceptor::MaybeProxyRequestHandler(
    content::BrowserContext* browser_context,
    SearchPrefetchURLLoader::RequestHandler prefetched_loader_handler) {
  network::URLLoaderFactoryBuilder factory_builder;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  CHECK(web_contents);
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();

  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  if (web_request_api) {
    web_request_api->MaybeProxyURLLoaderFactory(
        browser_context, render_frame_host,
        render_frame_host->GetProcess()->GetID(),
        content::ContentBrowserClient::URLLoaderFactoryType::kNavigation,
        navigation_id_, ukm::kInvalidSourceIdObj, factory_builder,
        /*header_client=*/nullptr, navigation_response_task_runner_,
        /*request_initiator=*/url::Origin());
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return base::BindOnce(
      &SearchPrefetchRequestHandler,
      std::move(factory_builder)
          .Finish(base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              std::move(prefetched_loader_handler))));
}

void SearchPrefetchURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SearchPrefetchURLLoader::RequestHandler prefetched_loader_handler =
      MaybeCreateLoaderForRequest(tentative_resource_request,
                                  frame_tree_node_id_);

  if (prefetched_loader_handler) {
    prefetched_loader_handler = MaybeProxyRequestHandler(
        browser_context, std::move(prefetched_loader_handler));
  }

  std::move(callback).Run(std::move(prefetched_loader_handler));
}
