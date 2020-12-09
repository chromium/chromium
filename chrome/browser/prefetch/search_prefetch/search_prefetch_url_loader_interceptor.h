// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

class SearchPrefetchURLLoader;

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

  // Creates a SearchPrefetchURLLoader if there is a prefetched response able to
  // be served to |tentative_resource_request|,.
  static std::unique_ptr<SearchPrefetchURLLoader> MaybeCreateLoaderForRequest(
      const network::ResourceRequest& tentative_resource_request,
      int frame_tree_node_id);

  // content::URLLaoderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  bool MaybeInterceptNavigation(
      const network::ResourceRequest& tentative_resource_request);

  // Used to get the current WebContents/Profile.
  const int frame_tree_node_id_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
