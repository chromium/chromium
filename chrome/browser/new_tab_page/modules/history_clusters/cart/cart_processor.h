// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_CART_CART_PROCESSOR_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_CART_CART_PROCESSOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"

class CartService;

class CartProcessor {
 public:
  explicit CartProcessor(CartService* cart_service);
  CartProcessor(const CartProcessor&) = delete;
  CartProcessor& operator=(const CartProcessor&) = delete;
  ~CartProcessor();

  // Get the most relevant cart for the given `cluster`.
  void GetCartForCluster(
      history_clusters::mojom::ClusterPtr cluster,
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback
          callback);

 private:
  void OnLoadCart(
      history_clusters::mojom::ClusterPtr cluster,
      ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback
          callback,
      bool success,
      std::vector<CartDB::KeyAndValue> carts);

  raw_ptr<CartService> cart_service_;
  base::WeakPtrFactory<CartProcessor> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_CART_CART_PROCESSOR_H_
