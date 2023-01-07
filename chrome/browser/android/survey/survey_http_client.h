// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SURVEY_SURVEY_HTTP_CLIENT_H_
#define CHROME_BROWSER_ANDROID_SURVEY_SURVEY_HTTP_CLIENT_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/android/survey/http_client_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace survey {

// Implementation class that used to send HTTP request to survey service.
class SurveyHttpClient {
 public:
  using ResponseCallback =
      base::OnceCallback<void(int32_t response_code,
                              int32_t net_error_code,
                              std::vector<uint8_t> response_bytes,
                              std::vector<std::string> response_header_keys,
                              std::vector<std::string> response_header_values)>;

  SurveyHttpClient(
      HttpClientType http_client_type,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~SurveyHttpClient();

  SurveyHttpClient(const SurveyHttpClient& client) = delete;
  SurveyHttpClient& operator=(const SurveyHttpClient& client) = delete;

  // Send a HTTP request to |url| of type |request_type|, with body
  // |request_body|, and headers assembled from |header_keys| and
  // |header_values|. The order of |header_keys| must match the order of
  // |header_values|. |callback| will be called when the request completes with
  // response or error.
  void Send(const GURL& gurl,
            const std::string& request_type,
            std::vector<uint8_t> request_body,
            std::vector<std::string> header_keys,
            std::vector<std::string> header_values,
            ResponseCallback callback);

 private:
  void OnSimpleLoaderComplete(ResponseCallback response_callback,
                              network::SimpleURLLoader* simple_loader,
                              std::unique_ptr<std::string> response);

  const HttpClientType http_client_type_;
  std::set<std::unique_ptr<network::SimpleURLLoader>, base::UniquePtrComparator>
      url_loaders_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
};

}  // namespace survey

#endif  // CHROME_BROWSER_ANDROID_SURVEY_SURVEY_HTTP_CLIENT_H_
