// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_HTTP_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_HTTP_FETCHER_H_

#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {
namespace quick_pair {

using FetchCompleteCallback =
    base::OnceCallback<void(std::unique_ptr<std::string>,
                            std::unique_ptr<FastPairHttpResult>)>;

// Makes HTTP GET requests and returns the response.
class HttpFetcher {
 public:
  HttpFetcher();
  HttpFetcher(const HttpFetcher&) = delete;
  HttpFetcher& operator=(const HttpFetcher&) = delete;
  virtual ~HttpFetcher();

  // Performs a GET request to the desired URL and returns the response, if
  // available, as a string to the provided |callback|.
  virtual void ExecuteGetRequest(const GURL& url,
                                 FetchCompleteCallback callback) = 0;

  // Performs a POST request to the desired URL and returns the response, if
  // available, as a string to the provided |callback|.
  virtual void ExecutePostRequest(const GURL& url,
                                  const std::string& body,
                                  FetchCompleteCallback callback);

  // Performs a DELETE request to the desired URL and returns the response, if
  // available, as a string to the provided |callback|.
  virtual void ExecuteDeleteRequest(const GURL& url,
                                    FetchCompleteCallback callback);

 protected:
  enum class RequestType {
    GET = 0,
    POST = 1,
    DELETE = 2,
  };
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_HTTP_FETCHER_H_
