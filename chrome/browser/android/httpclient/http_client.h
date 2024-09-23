// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HTTPCLIENT_HTTP_CLIENT_H_
#define CHROME_BROWSER_ANDROID_HTTPCLIENT_HTTP_CLIENT_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {

class SimpleURLLoader;
class SharedURLLoaderFactory;

}  // namespace network

namespace httpclient {

// Implementation class that used to send HTTP requests.
class HttpClient {
 public:
  using ResponseCallback = base::OnceCallback<void(
      int32_t response_code,
      int32_t net_error_code,
      std::vector<uint8_t>&& response_bytes,
      std::map<std::string, std::string>&& response_headers)>;

  explicit HttpClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~HttpClient();

  HttpClient(const HttpClient& client) = delete;
  HttpClient& operator=(const HttpClient& client) = delete;

  // Send a HTTP request to |url| of type |request_type|, with body
  // |request_body|, and headers assembled from |headers|. |callback| will be
  // called when the request completes with response or error.
  void Send(const GURL& gurl,
            const std::string& request_type,
            std::vector<uint8_t>&& request_body,
            std::map<std::string, std::string>&& headers,
            const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
            ResponseCallback callback);

 private:
  void OnSimpleLoaderComplete(ResponseCallback response_callback,
                              network::SimpleURLLoader* simple_loader,
                              std::unique_ptr<std::string> response);
  void ReleaseUrlLoader(network::SimpleURLLoader* simple_loader);

  std::set<std::unique_ptr<network::SimpleURLLoader>, base::UniquePtrComparator>
      url_loaders_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  base::WeakPtrFactory<HttpClient> weak_ptr_factory_{this};
};

}  // namespace httpclient

#endif  // CHROME_BROWSER_ANDROID_HTTPCLIENT_HTTP_CLIENT_H_
