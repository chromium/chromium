// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FULL_BODY_SEARCH_PREFETCH_REQUEST_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FULL_BODY_SEARCH_PREFETCH_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/prefetch/search_prefetch/base_search_prefetch_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class Profile;
class SearchPrefetchURLLoader;
class PrefetchedResponseContainer;

class FullBodySearchPrefetchRequest : public BaseSearchPrefetchRequest {
 public:
  FullBodySearchPrefetchRequest(const GURL& prefetch_url,
                                base::OnceClosure report_error_callback);
  ~FullBodySearchPrefetchRequest() override;

  FullBodySearchPrefetchRequest(const FullBodySearchPrefetchRequest&) = delete;
  FullBodySearchPrefetchRequest& operator=(
      const FullBodySearchPrefetchRequest&) = delete;

  // BaseSearchPrefetchRequest:
  void StartPrefetchRequestInternal(
      Profile* profile,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation)
      override;
  void StopPrefetch() override;
  std::unique_ptr<SearchPrefetchURLLoader> TakeSearchPrefetchURLLoader()
      override;

 private:
  // Called as a callback when the prefetch request is complete. Stores the
  // response and other metadata in |prefetch_response_container_|.
  void LoadDone(std::unique_ptr<std::string> response_body);

  // The ongoing prefetch request. Null before and after the fetch.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // Once a prefetch is completed successfully, the associated prefetch data
  // and metadata about the request.
  std::unique_ptr<PrefetchedResponseContainer> prefetch_response_container_;
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FULL_BODY_SEARCH_PREFETCH_REQUEST_H_
