// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_

#include "base/sequence_checker.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {
class BrowserContext;
}  // namespace content

// Intercepts search navigations that were previously prefetched.
class SearchPrefetchURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  SearchPrefetchURLLoaderInterceptor(
      content::FrameTreeNodeId frame_tree_node_id,
      int64_t navigation_id,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner);
  ~SearchPrefetchURLLoaderInterceptor() override;

  SearchPrefetchURLLoaderInterceptor(
      const SearchPrefetchURLLoaderInterceptor&) = delete;
  SearchPrefetchURLLoaderInterceptor& operator=(
      const SearchPrefetchURLLoaderInterceptor&) = delete;

  // Returns a valid handler if there is a prefetched response able to
  // be served to |tentative_resource_request|.
  static SearchPrefetchURLLoader::RequestHandler MaybeCreateLoaderForRequest(
      const network::ResourceRequest& tentative_resource_request,
      content::FrameTreeNodeId frame_tree_node_id);

  // content::URLLoaderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  // Maybe proxies the given request handler with the Extensions Web Request
  // API.
  SearchPrefetchURLLoader::RequestHandler MaybeProxyRequestHandler(
      content::BrowserContext* browser_context,
      SearchPrefetchURLLoader::RequestHandler prefetched_loader_handler);

  // Used to get the current WebContents/Profile.
  const content::FrameTreeNodeId frame_tree_node_id_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // These are sent to the Extensions Web Request API when maybe proxying the
  // prefetch URL loader.
  int64_t navigation_id_;
  scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner_;
#endif

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
