// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/digital_goods_factory_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/apps/digital_goods/digital_goods_impl.h"
#include "chrome/browser/apps/digital_goods/util.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payments_experimental_features.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace {

constexpr char kSupportedPaymentMethod[] = "https://play.google.com/billing";

}  // namespace

namespace apps {

// Public methods:
DigitalGoodsFactoryImpl::~DigitalGoodsFactoryImpl() = default;

// static
void DigitalGoodsFactoryImpl::BindDigitalGoodsFactory(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver) {
  DigitalGoodsFactoryImpl::GetOrCreateForCurrentDocument(render_frame_host)
      ->BindRequest(std::move(receiver));
}

// Creates a DigitalGoodsImpl instance.
void DigitalGoodsFactoryImpl::CreateDigitalGoods(
    const std::string& payment_method,
    CreateDigitalGoodsCallback callback) {
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kPayment)) {
    receiver_.ReportBadMessage("Feature policy blocks Payment");
    return;
  }

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

  if (apps::GetTwaPackageName(&render_frame_host()).empty()) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kUnsupportedContext,
        /*digital_goods=*/mojo::NullRemote());
    return;
  }

  // TODO(jshikaram): check with Android if there is a payment_method available.
  std::move(callback).Run(
      payments::mojom::CreateDigitalGoodsResponseCode::kOk,
      DigitalGoodsImpl::CreateAndBind(&render_frame_host()));
}

// Private methods:

DigitalGoodsFactoryImpl::DigitalGoodsFactoryImpl(content::RenderFrameHost* rfh)
    : content::DocumentUserData<DigitalGoodsFactoryImpl>(rfh),
      receiver_(this) {}

void DigitalGoodsFactoryImpl::BindRequest(
    mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver) {
  receiver_.Bind(std::move(receiver));
}

DOCUMENT_USER_DATA_KEY_IMPL(DigitalGoodsFactoryImpl);

}  // namespace apps
