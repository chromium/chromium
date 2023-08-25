// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_DISCOUNT_DISCOUNT_PROCESSOR_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_DISCOUNT_DISCOUNT_PROCESSOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/discount/discount.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "components/commerce/core/shopping_service.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"

class DiscountProcessor {
 public:
  explicit DiscountProcessor(commerce::ShoppingService* shopping_service);
  DiscountProcessor(const DiscountProcessor&) = delete;
  DiscountProcessor& operator=(const DiscountProcessor&) = delete;
  ~DiscountProcessor();

  // Get discounts for the given `cluster`.
  void GetDiscountsForCluster(
      history_clusters::mojom::ClusterPtr cluster,
      ntp::history_clusters::mojom::PageHandler::GetDiscountsForClusterCallback
          callback);

 private:
  void CallbackWithDiscountData(
      ntp::history_clusters::mojom::PageHandler::GetDiscountsForClusterCallback
          callback,
      const commerce::DiscountsMap& discounts_map);

  raw_ptr<commerce::ShoppingService> shopping_service_;
  base::WeakPtrFactory<DiscountProcessor> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_DISCOUNT_DISCOUNT_PROCESSOR_H_
