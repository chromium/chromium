// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_BASE_SEARCH_PREFETCH_REQUEST_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_BASE_SEARCH_PREFETCH_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

class Profile;
class SearchPrefetchURLLoader;

enum class SearchPrefetchStatus {
  // The request has not started yet. This status should ideally never be
  // recorded as Start() should be called on the same stack as creating the
  // fetcher (as of now).
  kNotStarted = 0,
  // The request is on the network and may move to any other state.
  kInFlight = 1,
  // The request received all the data and is ready to serve.
  kSuccessfullyCompleted = 2,
  // The request hit an error and cannot be served.
  kRequestFailed = 3,
  // The request was cancelled before completion.
  kRequestCancelled = 4,
};

// A class representing a prefetch used by the Search Prefetch Service.
// Implementors should provide the fetch and storage functionality as well as
// updating |current_status_|.
class BaseSearchPrefetchRequest {
 public:
  BaseSearchPrefetchRequest(const GURL& prefetch_url,
                            base::OnceClosure report_error_callback);
  virtual ~BaseSearchPrefetchRequest();

  BaseSearchPrefetchRequest(const BaseSearchPrefetchRequest&) = delete;
  BaseSearchPrefetchRequest& operator=(const BaseSearchPrefetchRequest&) =
      delete;

  // Starts the network request to prefetch |prefetch_url_|. Sets various fields
  // on a resource request and calls |StartPrefetchRequestInternal()|.
  void StartPrefetchRequest(Profile* profile);

  // Starts and begins processing |resource_request|.
  virtual void StartPrefetchRequestInternal(
      Profile* profile,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;

  // Cancels the on-going prefetch and should mark |current_status_|
  // appropriately.
  virtual void CancelPrefetch() = 0;

  SearchPrefetchStatus current_status() const { return current_status_; }

  const GURL& prefetch_url() const { return prefetch_url_; }

  // Takes ownership of underlying data/objects needed to serve the response.
  virtual std::unique_ptr<SearchPrefetchURLLoader>
  TakeSearchPrefetchURLLoader() = 0;

 protected:
  SearchPrefetchStatus current_status_ = SearchPrefetchStatus::kNotStarted;

  // The URL to prefetch the search terms from.
  const GURL prefetch_url_;

  // Called when there is a network/server error on the prefetch request.
  base::OnceClosure report_error_callback_;
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_BASE_SEARCH_PREFETCH_REQUEST_H_
