// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_PAY_ARC_DIGITAL_GOODS_BRIDGE_H_
#define ASH_COMPONENTS_ARC_PAY_ARC_DIGITAL_GOODS_BRIDGE_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/digital_goods.mojom.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Queries a TWA's digital goods service.
class ArcDigitalGoodsBridge : public KeyedService {
 public:
  using GetDetailsCallback =
      base::OnceCallback<void(payments::mojom::BillingResponseCode,
                              std::vector<payments::mojom::ItemDetailsPtr>)>;
  using AcknowledgeCallback =
      base::OnceCallback<void(payments::mojom::BillingResponseCode)>;
  using ListPurchasesCallback = base::OnceCallback<void(
      payments::mojom::BillingResponseCode,
      std::vector<payments::mojom::PurchaseReferencePtr>)>;
  using ListPurchaseHistoryCallback = base::OnceCallback<void(
      payments::mojom::BillingResponseCode,
      std::vector<payments::mojom::PurchaseReferencePtr>)>;
  using ConsumeCallback =
      base::OnceCallback<void(payments::mojom::BillingResponseCode)>;

  // Returns the instance owned by the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcDigitalGoodsBridge* GetForBrowserContext(
      content::BrowserContext* context);

  // Used only in testing with a TestBrowserContext.
  static ArcDigitalGoodsBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcDigitalGoodsBridge(content::BrowserContext* browser_context,
                        ArcBridgeService* bridge_service);
  ~ArcDigitalGoodsBridge() override;

  // Disallow copy and assign.
  ArcDigitalGoodsBridge(const ArcDigitalGoodsBridge& other) = delete;
  ArcDigitalGoodsBridge& operator=(const ArcDigitalGoodsBridge& other) = delete;

  // Queries a specific package for SKU details by item IDs.
  void GetDetails(const std::string& package_name,
                  const std::string& scope,
                  const std::vector<std::string>& item_ids,
                  GetDetailsCallback callback);

  // Informs a package that the purchase identified by |purchase_token| was
  // successfully acknowledged.
  void Acknowledge(const std::string& package_name,
                   const std::string& scope,
                   const std::string& purchase_token,
                   bool make_available_again,
                   AcknowledgeCallback callback);

  // Queries a package for information on all items that are currently owned by
  // the user. May include unconfirmed purchases.
  void ListPurchases(const std::string& package_name,
                     const std::string& scope,
                     ListPurchasesCallback callback);

  // Queries a package for information on the latest purchase for each item type
  // ever purchased by the user. May include expired or consumed purchases.
  void ListPurchaseHistory(const std::string& package_name,
                           const std::string& scope,
                           ListPurchaseHistoryCallback callback);

  // Informs a package that the purchase identified by |purchase_token| was used
  // up.
  void Consume(const std::string& package_name,
               const std::string& scope,
               const std::string& purchase_token,
               ConsumeCallback callback);

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_PAY_ARC_DIGITAL_GOODS_BRIDGE_H_
