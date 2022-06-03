// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_REQUEST_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/prefetch/search_prefetch/base_search_prefetch_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

class Profile;
class SearchPrefetchURLLoader;
class StreamingSearchPrefetchURLLoader;

// A class that can serve a prefetch that is still being streamed into the
// client. As long as the headers and body start have been received, the
// response can start to be served. This class serves as a container for a
// StreamingSearchPrefetchURLLoader, to support |TakeSearchPrefetchURLLoader()|
// more easily.
class StreamingSearchPrefetchRequest : public BaseSearchPrefetchRequest {
 public:
  StreamingSearchPrefetchRequest(const GURL& prefetch_url,
                                 base::OnceClosure report_error_callback);
  ~StreamingSearchPrefetchRequest() override;

  StreamingSearchPrefetchRequest(const StreamingSearchPrefetchRequest&) =
      delete;
  StreamingSearchPrefetchRequest& operator=(
      const StreamingSearchPrefetchRequest&) = delete;

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
  // The ongoing prefetch request. Null before and after the fetch.
  std::unique_ptr<StreamingSearchPrefetchURLLoader> streaming_url_loader_;
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_STREAMING_SEARCH_PREFETCH_REQUEST_H_
