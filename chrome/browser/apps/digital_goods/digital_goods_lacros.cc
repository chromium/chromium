// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/digital_goods_lacros.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/digital_goods/mojom/digital_goods.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace apps {

namespace {

std::optional<std::pair<webapps::AppId, GURL>> GetWebAppIdAndScopeForDocument(
    content::RenderFrameHost& render_frame_host) {
  web_app::WebAppProvider* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext()));
  if (!provider) {
    return std::nullopt;
  }

  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::optional<webapps::AppId> app_id = registrar.FindAppWithUrlInScope(
      render_frame_host.GetMainFrame()->GetLastCommittedURL());
  if (!app_id) {
    return std::nullopt;
  }
  return std::make_pair(*app_id, registrar.GetAppScope(*app_id));
}

}  // namespace

// DigitalGoodsLacros implementation.

DigitalGoodsLacros::~DigitalGoodsLacros() = default;

mojo::PendingRemote<payments::mojom::DigitalGoods>
DigitalGoodsLacros::BindRequest() {
  mojo::PendingRemote<payments::mojom::DigitalGoods> pending_remote;
  mojo::PendingReceiver<payments::mojom::DigitalGoods> pending_receiver =
      pending_remote.InitWithNewPipeAndPassReceiver();
  receiver_set_.Add(this, std::move(pending_receiver));
  return pending_remote;
}

void DigitalGoodsLacros::GetDetails(const std::vector<std::string>& item_ids,
                                    GetDetailsCallback callback) {
  auto id_and_scope = GetWebAppIdAndScopeForDocument(render_frame_host());
  if (!id_and_scope) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*item_detail_list=*/{});
    return;
  }
  const auto& [app_id, scope] = *id_and_scope;
  digital_goods_->GetDetails(app_id, scope, item_ids, std::move(callback));
}

void DigitalGoodsLacros::ListPurchases(ListPurchasesCallback callback) {
  auto id_and_scope = GetWebAppIdAndScopeForDocument(render_frame_host());
  if (!id_and_scope) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }
  const auto& [app_id, scope] = *id_and_scope;
  digital_goods_->ListPurchases(app_id, scope, std::move(callback));
}

void DigitalGoodsLacros::ListPurchaseHistory(
    ListPurchaseHistoryCallback callback) {
  auto id_and_scope = GetWebAppIdAndScopeForDocument(render_frame_host());
  if (!id_and_scope) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable,
        /*purchase_reference_list=*/{});
    return;
  }
  const auto& [app_id, scope] = *id_and_scope;
  digital_goods_->ListPurchaseHistory(app_id, scope, std::move(callback));
}

void DigitalGoodsLacros::Consume(const std::string& purchase_token,
                                 ConsumeCallback callback) {
  auto id_and_scope = GetWebAppIdAndScopeForDocument(render_frame_host());
  if (!id_and_scope) {
    std::move(callback).Run(
        payments::mojom::BillingResponseCode::kClientAppUnavailable);
    return;
  }
  const auto& [app_id, scope] = *id_and_scope;
  digital_goods_->Consume(app_id, scope, purchase_token, std::move(callback));
}

DigitalGoodsLacros::DigitalGoodsLacros(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingRemote<crosapi::mojom::DigitalGoods> remote)
    : content::DocumentUserData<DigitalGoodsLacros>(render_frame_host),
      digital_goods_(std::move(remote)) {}

DOCUMENT_USER_DATA_KEY_IMPL(DigitalGoodsLacros);

// DigitalGoodsFactoryLacros implementation.

DigitalGoodsFactoryLacros::~DigitalGoodsFactoryLacros() = default;

// static
void DigitalGoodsFactoryLacros::Bind(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver) {
  DigitalGoodsFactoryLacros::GetOrCreateForCurrentDocument(render_frame_host)
      ->BindRequest(std::move(receiver));
}

void DigitalGoodsFactoryLacros::CreateDigitalGoods(
    const std::string& payment_method,
    CreateDigitalGoodsCallback callback) {
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kPayment)) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kUnsupportedContext,
        mojo::NullRemote());
    return;
  }

  if (auto* digital_goods =
          DigitalGoodsLacros::GetForCurrentDocument(&render_frame_host())) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kOk,
        digital_goods->BindRequest());
    return;
  }

  auto id_and_scope = GetWebAppIdAndScopeForDocument(render_frame_host());
  if (!id_and_scope) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kUnsupportedContext,
        mojo::NullRemote());
    return;
  }
  const auto& [app_id, scope] = *id_and_scope;

  auto* lacros_service = chromeos::LacrosService::Get();
  if (!(lacros_service &&
        lacros_service->IsAvailable<crosapi::mojom::DigitalGoodsFactory>())) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kError,
        mojo::NullRemote());
    return;
  }

  pending_callbacks_.push_back(std::move(callback));
  if (pending_callbacks_.size() > 1) {
    // A crosapi call is already in flight, just wait for it to return.
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::DigitalGoodsFactory>()
      ->CreateDigitalGoods(
          payment_method, app_id,
          base::BindOnce(&DigitalGoodsFactoryLacros::OnCreateDigitalGoods,
                         weak_ptr_factory_.GetWeakPtr()));
}

DigitalGoodsFactoryLacros::DigitalGoodsFactoryLacros(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<DigitalGoodsFactoryLacros>(render_frame_host) {}

void DigitalGoodsFactoryLacros::BindRequest(
    mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver) {
  receiver_.Bind(std::move(receiver));
}

void DigitalGoodsFactoryLacros::OnCreateDigitalGoods(
    payments::mojom::CreateDigitalGoodsResponseCode code,
    mojo::PendingRemote<crosapi::mojom::DigitalGoods> remote) {
  if (code != payments::mojom::CreateDigitalGoodsResponseCode::kOk) {
    for (auto& callback : pending_callbacks_) {
      std::move(callback).Run(code, mojo::NullRemote());
    }
    pending_callbacks_.clear();
    return;
  }
  DigitalGoodsLacros::CreateForCurrentDocument(&render_frame_host(),
                                               std::move(remote));
  auto* digital_goods =
      DigitalGoodsLacros::GetForCurrentDocument(&render_frame_host());
  DCHECK(digital_goods);
  for (auto& callback : pending_callbacks_) {
    std::move(callback).Run(
        payments::mojom::CreateDigitalGoodsResponseCode::kOk,
        digital_goods->BindRequest());
  }
  pending_callbacks_.clear();
}

DOCUMENT_USER_DATA_KEY_IMPL(DigitalGoodsFactoryLacros);

}  // namespace apps
