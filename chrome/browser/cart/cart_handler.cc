// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_handler.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "components/search/ntp_features.h"

CartHandler::CartHandler(
    mojo::PendingReceiver<chrome_cart::mojom::CartHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      cart_service_(CartServiceFactory::GetForProfile(profile)) {}

CartHandler::~CartHandler() = default;

void CartHandler::GetMerchantCarts(GetMerchantCartsCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule));
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleDataParam) == "fake") {
    cart_service_->LoadCartsWithFakeData(
        base::BindOnce(&CartHandler::GetCartDataCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    cart_service_->LoadAllActiveCarts(
        base::BindOnce(&CartHandler::GetCartDataCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void CartHandler::HideCartModule() {
  cart_service_->Hide();
}

void CartHandler::RestoreHiddenCartModule() {
  cart_service_->RestoreHidden();
}

void CartHandler::HideCart(const GURL& cart_url, HideCartCallback callback) {
  cart_service_->HideCart(cart_url, std::move(callback));
}

void CartHandler::RestoreHiddenCart(const GURL& cart_url,
                                    RestoreHiddenCartCallback callback) {
  cart_service_->RestoreHiddenCart(cart_url, std::move(callback));
}

void CartHandler::RemoveCart(const GURL& cart_url,
                             RemoveCartCallback callback) {
  cart_service_->RemoveCart(cart_url, std::move(callback));
}

void CartHandler::RestoreRemovedCart(const GURL& cart_url,
                                     RestoreRemovedCartCallback callback) {
  cart_service_->RestoreRemovedCart(cart_url, std::move(callback));
}

void CartHandler::GetCartDataCallback(GetMerchantCartsCallback callback,
                                      bool success,
                                      std::vector<CartDB::KeyAndValue> res) {
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  for (CartDB::KeyAndValue proto_pair : res) {
    auto cart = chrome_cart::mojom::MerchantCart::New();
    cart->merchant = std::move(proto_pair.second.merchant());
    cart->cart_url = GURL(std::move(proto_pair.second.merchant_cart_url()));
    std::vector<std::string> image_urls;
    // Not show product images when showing welcome surface.
    if (!cart_service_->ShouldShowWelcomeSurface()) {
      for (std::string image_url : proto_pair.second.product_image_urls()) {
        cart->product_image_urls.emplace_back(std::move(image_url));
      }
      cart->discount_text =
          std::move(proto_pair.second.discount_info().discount_text());
    }
    carts.push_back(std::move(cart));
  }
  if (carts.size() > 0) {
    cart_service_->IncreaseWelcomeSurfaceCounter();
  }
  std::move(callback).Run(std::move(carts));
}

void CartHandler::GetWarmWelcomeVisible(
    GetWarmWelcomeVisibleCallback callback) {
  std::move(callback).Run(cart_service_->ShouldShowWelcomeSurface());
}

// TODO(crbug.com/1174281): Below metrics collection can be moved to JS to avoid
// cross-process calls.
void CartHandler::OnCartItemClicked(uint32_t index) {
  base::UmaHistogramCounts100("NewTabPage.Carts.ClickCart", index);
}

void CartHandler::OnModuleCreated(uint32_t count) {
  base::UmaHistogramCounts100("NewTabPage.Carts.CartCount", count);
}

void CartHandler::GetDiscountConsentCardVisible(
    GetDiscountConsentCardVisibleCallback callback) {
  std::move(callback).Run(cart_service_->ShouldShowDiscountConsent());
}

void CartHandler::OnDiscountConsentAcknowledged(bool accept) {
  cart_service_->AcknowledgeDiscountConsent(accept);
}

void CartHandler::GetDiscountEnabled(GetDiscountEnabledCallback callback) {
  std::move(callback).Run(cart_service_->IsCartDiscountEnabled());
}

void CartHandler::SetDiscountEnabled(bool enabled) {
  cart_service_->SetCartDiscountEnabled(enabled);
}
