// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_UNAUTHENTICATED_HTTP_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_UNAUTHENTICATED_HTTP_FETCHER_H_

#include "ash/quick_pair/repository/http_fetcher.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ash {
namespace quick_pair {

// Makes HTTP GET requests and returns the response.
class UnauthenticatedHttpFetcher : public HttpFetcher {
 public:
  explicit UnauthenticatedHttpFetcher(
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  UnauthenticatedHttpFetcher(const UnauthenticatedHttpFetcher&) = delete;
  UnauthenticatedHttpFetcher& operator=(const UnauthenticatedHttpFetcher&) =
      delete;
  ~UnauthenticatedHttpFetcher() override;

  // Performs a GET request to the desired URL and returns the response, if
  // available, as a string to the provided |callback|.
  void ExecuteGetRequest(const GURL& url,
                         FetchCompleteCallback callback) override;

 private:
  void OnComplete(std::unique_ptr<network::SimpleURLLoader> simple_loader,
                  FetchCompleteCallback success_callback,
                  std::unique_ptr<std::string> response_body);

  net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::WeakPtrFactory<UnauthenticatedHttpFetcher> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_UNAUTHENTICATED_HTTP_FETCHER_H_
