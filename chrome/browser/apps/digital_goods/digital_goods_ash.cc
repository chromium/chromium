// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/digital_goods_ash.h"

#include <utility>

#include "ash/components/arc/pay/arc_digital_goods_bridge.h"
#include "chrome/browser/apps/digital_goods/util.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/digital_goods/mojom/digital_goods.mojom.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payments_experimental_features.h"
#include "url/gurl.h"

namespace apps {

namespace {

constexpr char kSupportedPaymentMethod[] = "https://play.google.com/billing";

// Gets the package name of the Android app linked to this web app.
std::optional<std::string> GetTwaPackageName(const std::string& app_id) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* apk_web_app_service = ash::ApkWebAppService::Get(profile);
  if (!apk_web_app_service) {
    return std::nullopt;
  }
  return apk_web_app_service->GetPackageNameForWebApp(app_id);
}

arc::ArcDigitalGoodsBridge* GetArcDigitalGoodsBridge() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return arc::ArcDigitalGoodsBridge::GetForBrowserContext(profile);
}

}  // namespace

// DigitalGoodsAsh implementation.

DigitalGoodsAsh::DigitalGoodsAsh() = default;
DigitalGoodsAsh::~DigitalGoodsAsh() = default;

mojo::PendingRemote<crosapi::mojom::DigitalGoods>
DigitalGoodsAsh::BindRequest() {
  mojo::PendingRemote<crosapi::mojom::DigitalGoods> pending_remote;
  mojo::PendingReceiver<crosapi::mojom::DigitalGoods> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  receiver_set_.Add(this, std::move(pending_receiver));
  return pending_remote;
}

void DigitalGoodsAsh::GetDetails(const std::string& web_app_id,
                                 const GURL& scope,
                                 const std::vector<std::string>& item_ids,
                                 GetDetailsCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*item_detail_list=*/{});
    return;
  }

  std::optional<std::string> package_name = GetTwaPackageName(web_app_id);
  if (!package_name) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*item_detail_list=*/{});
    return;
  }

  digital_goods_service->GetDetails(*package_name, scope.spec(), item_ids,
                                    std::move(callback));
}

void DigitalGoodsAsh::ListPurchases(const std::string& web_app_id,
                                    const GURL& scope,
                                    ListPurchasesCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  std::optional<std::string> package_name = GetTwaPackageName(web_app_id);
  if (!package_name) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  digital_goods_service->ListPurchases(*package_name, scope.spec(),
                                       std::move(callback));
}

void DigitalGoodsAsh::ListPurchaseHistory(
    const std::string& web_app_id,
    const GURL& scope,
    ListPurchaseHistoryCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  std::optional<std::string> package_name = GetTwaPackageName(web_app_id);
  if (!package_name) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  digital_goods_service->ListPurchaseHistory(*package_name, scope.spec(),
                                             std::move(callback));
}

void DigitalGoodsAsh::Consume(const std::string& web_app_id,
                              const GURL& scope,
                              const std::string& purchase_token,
                              ConsumeCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable);
    return;
  }

  std::optional<std::string> package_name = GetTwaPackageName(web_app_id);
  if (!package_name) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable);
    return;
  }

  digital_goods_service->Consume(*package_name, scope.spec(), purchase_token,
                                 std::move(callback));
}

// DigitalGoodsFactoryAsh implementation.

DigitalGoodsFactoryAsh::DigitalGoodsFactoryAsh() = default;
DigitalGoodsFactoryAsh::~DigitalGoodsFactoryAsh() = default;

void DigitalGoodsFactoryAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::DigitalGoodsFactory> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void DigitalGoodsFactoryAsh::CreateDigitalGoods(
    const std::string& payment_method,
    const std::string& web_app_id,
    CreateDigitalGoodsCallback callback) {
  // Check feature flag.
  if (!payments::PaymentsExperimentalFeatures::IsEnabled(
          payments::features::kAppStoreBilling)) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kUnsupportedContext,
        /*digital_goods=*/mojo::NullRemote());
    return;
  }

  if (payment_method != kSupportedPaymentMethod) {
    std::move(callback).Run(payments::mojom::CreateDigitalGoodsResponseCode::
                                kUnsupportedPaymentMethod,
                            /*digital_goods=*/mojo::NullRemote());
    return;
  }

  if (!GetTwaPackageName(web_app_id)) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kUnsupportedContext,
        /*digital_goods=*/mojo::NullRemote());
    return;
  }

  std::move(callback).Run(payments::mojom::CreateDigitalGoodsResponseCode::kOk,
                          /*digital_goods=*/digital_goods_.BindRequest());
}

}  // namespace apps
