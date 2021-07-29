// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_HTTP_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_HTTP_FETCHER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace quick_pair {

using FetchCompleteCallback =
    base::OnceCallback<void(std::unique_ptr<std::string>)>;

// Makes HTTP GET requests and returns the response.
class HttpFetcher {
 public:
  explicit HttpFetcher(
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  HttpFetcher(const HttpFetcher&) = delete;
  HttpFetcher& operator=(const HttpFetcher&) = delete;
  virtual ~HttpFetcher();

  // Performs a GET request to the desired URL and returns the response, if
  // available, as a string to the provided |callback|.
  virtual void ExecuteGetRequest(const GURL& url,
                                 FetchCompleteCallback callback);

 private:
  void OnComplete(std::unique_ptr<network::SimpleURLLoader> simple_loader,
                  FetchCompleteCallback success_callback,
                  std::unique_ptr<std::string> response_body);
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::WeakPtrFactory<HttpFetcher> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_HTTP_FETCHER_H_
