// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/digital_goods_impl.h"

#include "chrome/browser/apps/digital_goods/util.h"
#include "components/digital_goods/mojom/digital_goods.mojom.h"
#include "content/public/browser/render_frame_host.h"

namespace {

void LogErrorState(const std::string& package_name, const std::string& scope) {
  DVLOG(1) << "DGAPI call failed because package name or scope was empty.";
  DVLOG(1) << "Package Name: " << package_name;
  DVLOG(1) << "Scope: " << scope;
}

}  // namespace

namespace apps {

// Public methods:
DigitalGoodsImpl::~DigitalGoodsImpl() = default;

// static
mojo::PendingRemote<payments::mojom::DigitalGoods>
DigitalGoodsImpl::CreateAndBind(content::RenderFrameHost* render_frame_host) {
  return DigitalGoodsImpl::GetOrCreateForCurrentDocument(render_frame_host)
      ->BindRequest();
}

mojo::PendingRemote<payments::mojom::DigitalGoods>
DigitalGoodsImpl::BindRequest() {
  mojo::PendingRemote<payments::mojom::DigitalGoods> pending_remote;
  mojo::PendingReceiver<payments::mojom::DigitalGoods> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  receiver_set_.Add(this, std::move(pending_receiver));
  return pending_remote;
}

void DigitalGoodsImpl::GetDetails(const std::vector<std::string>& item_ids,
                                  GetDetailsCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*item_detail_list=*/{});
    return;
  }

  const std::string package_name =
      apps::GetTwaPackageName(&render_frame_host());
  const std::string scope = apps::GetScope(&render_frame_host());
  if (package_name.empty() || scope.empty()) {
    LogErrorState(package_name, scope);
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*item_detail_list=*/{});
    return;
  }

  digital_goods_service->GetDetails(package_name, scope, item_ids,
                                    std::move(callback));
}

void DigitalGoodsImpl::ListPurchases(ListPurchasesCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  const std::string package_name =
      apps::GetTwaPackageName(&render_frame_host());
  const std::string scope = apps::GetScope(&render_frame_host());
  if (package_name.empty() || scope.empty()) {
    LogErrorState(package_name, scope);
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  digital_goods_service->ListPurchases(package_name, scope,
                                       std::move(callback));
}

void DigitalGoodsImpl::ListPurchaseHistory(
    ListPurchaseHistoryCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  const std::string package_name =
      apps::GetTwaPackageName(&render_frame_host());
  const std::string scope = apps::GetScope(&render_frame_host());
  if (package_name.empty() || scope.empty()) {
    LogErrorState(package_name, scope);
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }

  digital_goods_service->ListPurchaseHistory(package_name, scope,
                                             std::move(callback));
}

void DigitalGoodsImpl::Consume(const std::string& purchase_token,
                               ConsumeCallback callback) {
  auto* digital_goods_service = GetArcDigitalGoodsBridge();

  if (!digital_goods_service) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable);
    return;
  }

  const std::string package_name =
      apps::GetTwaPackageName(&render_frame_host());
  const std::string scope = apps::GetScope(&render_frame_host());
  if (package_name.empty() || scope.empty()) {
    LogErrorState(package_name, scope);
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable);
    return;
  }

  digital_goods_service->Consume(package_name, scope, purchase_token,
                                 std::move(callback));
}

// Private methods:
DigitalGoodsImpl::DigitalGoodsImpl(content::RenderFrameHost* rfh)
    : content::DocumentUserData<DigitalGoodsImpl>(rfh) {}

arc::ArcDigitalGoodsBridge* DigitalGoodsImpl::GetArcDigitalGoodsBridge() {
  return arc::ArcDigitalGoodsBridge::GetForBrowserContext(
      render_frame_host().GetBrowserContext());
}

DOCUMENT_USER_DATA_KEY_IMPL(DigitalGoodsImpl);

}  // namespace apps
