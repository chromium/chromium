// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_

#include "base/sequence_checker.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {
class BrowserContext;
}  // namespace content

// Intercepts search navigations that were previously prefetched.
class SearchPrefetchURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  explicit SearchPrefetchURLLoaderInterceptor(int frame_tree_node_id);
  ~SearchPrefetchURLLoaderInterceptor() override;

  SearchPrefetchURLLoaderInterceptor(
      const SearchPrefetchURLLoaderInterceptor&) = delete;
  SearchPrefetchURLLoaderInterceptor& operator=(
      const SearchPrefetchURLLoaderInterceptor&) = delete;

  // Returns a valid handler if there is a prefetched response able to
  // be served to |tentative_resource_request|.
  static SearchPrefetchURLLoader::RequestHandler MaybeCreateLoaderForRequest(
      const network::ResourceRequest& tentative_resource_request,
      int frame_tree_node_id);

  // content::URLLoaderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  // Used to get the current WebContents/Profile.
  const int frame_tree_node_id_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
