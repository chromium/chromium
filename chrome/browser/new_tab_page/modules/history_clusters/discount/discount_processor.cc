// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/discount/discount_processor.h"

#include "base/containers/flat_map.h"
#include "components/commerce/core/commerce_constants.h"
#include "net/base/url_util.h"

namespace {
ntp::history_clusters::discount::mojom::DiscountPtr DiscountToMojom(
    GURL url,
    commerce::DiscountInfo discount_info) {
  auto discount_mojom = ntp::history_clusters::discount::mojom::Discount::New();
  url = net::AppendOrReplaceQueryParameter(url, commerce::kUTMSourceLabel,
                                           commerce::kUTMSourceValue);
  url = net::AppendOrReplaceQueryParameter(url, commerce::kUTMMediumLabel,
                                           commerce::kUTMMediumValue);
  url = net::AppendOrReplaceQueryParameter(
      url, commerce::kUTMCampaignLabel,
      commerce::kUTMCampaignValueForDiscounts);

  discount_mojom->annotated_visit_url = url;
  discount_mojom->value_in_text = discount_info.value_in_text;
  return discount_mojom;
}
}  // namespace

DiscountProcessor::DiscountProcessor(
    commerce::ShoppingService* shopping_service)
    : shopping_service_(shopping_service) {}

DiscountProcessor::~DiscountProcessor() = default;

void DiscountProcessor::GetDiscountsForCluster(
    history_clusters::mojom::ClusterPtr cluster,
    ntp::history_clusters::mojom::PageHandler::GetDiscountsForClusterCallback
        callback) {
  std::vector<GURL> urls;
  for (auto& visit : cluster->visits) {
    urls.emplace_back(visit->normalized_url);
  }

  shopping_service_->GetDiscountInfoForUrls(
      std::move(urls),
      base::BindOnce(&DiscountProcessor::CallbackWithDiscountData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DiscountProcessor::CallbackWithDiscountData(
    ntp::history_clusters::mojom::PageHandler::GetDiscountsForClusterCallback
        callback,
    const commerce::DiscountsMap& discounts_map) {
  auto result = base::flat_map<
      GURL, std::vector<ntp::history_clusters::discount::mojom::DiscountPtr>>();
  for (auto& entry : discounts_map) {
    auto discounts =
        std::vector<ntp::history_clusters::discount::mojom::DiscountPtr>();
    for (auto discount : entry.second) {
      discounts.emplace_back(DiscountToMojom(entry.first, discount));
    }
    result[entry.first] = std::move(discounts);
  }

  std::move(callback).Run(std::move(result));
}
