// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_ORIGIN_ROBOTS_RULES_CACHE_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_ORIGIN_ROBOTS_RULES_CACHE_H_

#include <string>

#include "base/containers/mru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/subresource_redirect/origin_robots_rules.h"
#include "url/origin.h"

class LitePagesServiceBypassDecider;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace subresource_redirect {

// Cache that maintains robots rules for multiple origins.
class OriginRobotsRulesCache {
 public:
  OriginRobotsRulesCache(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::WeakPtr<LitePagesServiceBypassDecider>
          litepages_service_bypass_decider);
  ~OriginRobotsRulesCache();

  OriginRobotsRulesCache(const OriginRobotsRulesCache&) = delete;
  OriginRobotsRulesCache& operator=(const OriginRobotsRulesCache&) = delete;

  // Gets the robots rules for the origin and invokes the callback with the
  // result. When the rules are missing in the cache, it will be fetched from
  // the LitePages service.
  void GetRobotsRules(const url::Origin& origin,
                      OriginRobotsRules::RobotsRulesReceivedCallback callback);

 private:
  // Cache keyed by origin
  base::MRUCache<url::Origin, std::unique_ptr<OriginRobotsRules>> rules_cache_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtr<LitePagesServiceBypassDecider>
      litepages_service_bypass_decider_;
};

}  // namespace subresource_redirect

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_ORIGIN_ROBOTS_RULES_CACHE_H_
