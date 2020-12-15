// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/origin_robots_rules_cache.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/subresource_redirect/litepages_service_bypass_decider.h"
#include "chrome/browser/subresource_redirect/origin_robots_rules.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace subresource_redirect {

OriginRobotsRulesCache::OriginRobotsRulesCache(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::WeakPtr<LitePagesServiceBypassDecider>
        litepages_service_bypass_decider)
    : rules_cache_(MaxOriginRobotsRulesCacheSize()),
      url_loader_factory_(std::move(url_loader_factory)),
      litepages_service_bypass_decider_(
          std::move(litepages_service_bypass_decider)) {}

OriginRobotsRulesCache::~OriginRobotsRulesCache() = default;

void OriginRobotsRulesCache::GetRobotsRules(
    const url::Origin& origin,
    OriginRobotsRules::RobotsRulesReceivedCallback callback) {
  DCHECK(!origin.opaque());
  if (!litepages_service_bypass_decider_ ||
      !litepages_service_bypass_decider_->ShouldAllowNow()) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  auto rules = rules_cache_.Get(origin);
  base::UmaHistogramBoolean(
      "SubresourceRedirect.RobotsRules.Browser.InMemoryCacheHit",
      rules != rules_cache_.end());
  if (rules == rules_cache_.end()) {
    // Create new rules fetcher
    rules = rules_cache_.Put(
        origin, std::make_unique<OriginRobotsRules>(
                    url_loader_factory_, origin,
                    base::BindOnce(&LitePagesServiceBypassDecider::
                                       NotifyFetchFailureWithResponseCode,
                                   litepages_service_bypass_decider_)));
  }
  rules->second->GetRobotsRules(std::move(callback));
}

}  // namespace subresource_redirect
