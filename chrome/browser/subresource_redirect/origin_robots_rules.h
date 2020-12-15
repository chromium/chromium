// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_ORIGIN_ROBOTS_RULES_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_ORIGIN_ROBOTS_RULES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "url/origin.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace subresource_redirect {

// Holds the robots rules for one origin. Fetches the robots rules on creation.
class OriginRobotsRules {
 public:
  // The callback to send the received robots rules. base::nullopt will be sent
  // when rule fetch fails.
  using RobotsRulesReceivedCallback =
      base::OnceCallback<void(base::Optional<std::string>)>;

  // The callback to notify 4xx, 5xx response codes. Sends the response code and
  // retry-after response header.
  using NotifyResponseErrorCallback =
      base::OnceCallback<void(int, base::TimeDelta)>;

  OriginRobotsRules(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const url::Origin& origin,
      NotifyResponseErrorCallback response_error_callback);
  ~OriginRobotsRules();

  // Get the robots rules for this origin. The callback is called immediately if
  // rules have been fetched. When rules fetch is in progress, the callback will
  // happen after it is complete.
  void GetRobotsRules(RobotsRulesReceivedCallback callback);

 private:
  // Holds the info pertaining to when robots rules are fetched.
  struct FetcherInfo {
    FetcherInfo(std::unique_ptr<network::SimpleURLLoader> url_loader,
                NotifyResponseErrorCallback response_error_callback);
    ~FetcherInfo();

    // Holds the URLLoader when robots rules are fetched.
    std::unique_ptr<network::SimpleURLLoader> url_loader;

    // Contains the requests that are pending for robots rules to be received.
    std::vector<RobotsRulesReceivedCallback> pending_callbacks;

    // Callback to notify response errors.
    NotifyResponseErrorCallback response_error_callback;
  };

  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // The received robots rules. Set when rules fetch completes successfully.
  base::Optional<std::string> robots_rules_;

  // Holds the robots rules fetcher state. Exists only when fetch is in
  // progress.
  std::unique_ptr<FetcherInfo> fetcher_info_;
};

}  // namespace subresource_redirect

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_ORIGIN_ROBOTS_RULES_H_
