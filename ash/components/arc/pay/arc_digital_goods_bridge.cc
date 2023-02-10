// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/pay/arc_digital_goods_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/no_destructor.h"
#include "components/digital_goods/mojom/digital_goods.mojom.h"

namespace arc {
namespace {

class ArcDigitalGoodsBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcDigitalGoodsBridge,
          ArcDigitalGoodsBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcDigitalGoodsBridgeFactory";

  static ArcDigitalGoodsBridgeFactory* GetInstance() {
    static base::NoDestructor<ArcDigitalGoodsBridgeFactory> factory;
    return factory.get();
  }

  ArcDigitalGoodsBridgeFactory() = default;
  ~ArcDigitalGoodsBridgeFactory() override = default;
};

}  // namespace

// static
ArcDigitalGoodsBridge* ArcDigitalGoodsBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcDigitalGoodsBridgeFactory::GetForBrowserContext(context);
}

// static
ArcDigitalGoodsBridge* ArcDigitalGoodsBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcDigitalGoodsBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcDigitalGoodsBridge::ArcDigitalGoodsBridge(
    content::BrowserContext* browser_context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {}

ArcDigitalGoodsBridge::~ArcDigitalGoodsBridge() = default;

void ArcDigitalGoodsBridge::GetDetails(const std::string& package_name,
                                       const std::string& scope,
                                       const std::vector<std::string>& item_ids,
                                       GetDetailsCallback callback) {
  mojom::DigitalGoodsInstance* digital_goods = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->digital_goods(), GetDetails);
  if (!digital_goods) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /* item_detail_list = */ {});
    return;
  }
  digital_goods->GetDetails(package_name, scope, item_ids, std::move(callback));
}

void ArcDigitalGoodsBridge::Acknowledge(const std::string& package_name,
                                        const std::string& scope,
                                        const std::string& purchase_token,
                                        bool make_available_again,
                                        AcknowledgeCallback callback) {
  mojom::DigitalGoodsInstance* digital_goods = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->digital_goods(), Acknowledge);
  if (!digital_goods) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable);
    return;
  }
  digital_goods->Acknowledge(package_name, scope, purchase_token,
                             make_available_again, std::move(callback));
}

void ArcDigitalGoodsBridge::ListPurchases(const std::string& package_name,
                                          const std::string& scope,
                                          ListPurchasesCallback callback) {
  mojom::DigitalGoodsInstance* digital_goods = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->digital_goods(), ListPurchases);
  if (!digital_goods) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /* purchase_reference_list = */ {});
    return;
  }
  digital_goods->ListPurchases(package_name, scope, std::move(callback));
}

void ArcDigitalGoodsBridge::ListPurchaseHistory(
    const std::string& package_name,
    const std::string& scope,
    ListPurchaseHistoryCallback callback) {
  mojom::DigitalGoodsInstance* digital_goods = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->digital_goods(), ListPurchaseHistory);
  if (!digital_goods) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /* purchase_reference_list = */ {});
    return;
  }
  digital_goods->ListPurchaseHistory(package_name, scope, std::move(callback));
}

void ArcDigitalGoodsBridge::Consume(const std::string& package_name,
                                    const std::string& scope,
                                    const std::string& purchase_token,
                                    ConsumeCallback callback) {
  mojom::DigitalGoodsInstance* digital_goods = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->digital_goods(), Consume);
  if (!digital_goods) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable);
    return;
  }
  digital_goods->Consume(package_name, scope, purchase_token,
                         std::move(callback));
}

// static
void ArcDigitalGoodsBridge::EnsureFactoryBuilt() {
  ArcDigitalGoodsBridgeFactory::GetInstance();
}

}  // namespace arc
