// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_handler.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "components/search/ntp_features.h"

CartHandler::CartHandler(
    mojo::PendingReceiver<chrome_cart::mojom::CartHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)), profile_(profile) {}

CartHandler::~CartHandler() = default;

void CartHandler::GetMerchantCarts(GetMerchantCartsCallback callback) {
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  // TODO(https://crbug.com/1157892): Replace this with a feature parameter for
  // fake data when real data is available.
  if (base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule)) {
    auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
    dummy_cart1->merchant = "Cart Foo";
    dummy_cart1->cart_url = GURL("https://www.google.com/");
    dummy_cart1->product_image_urls.emplace_back(
        "https://encrypted-tbn3.gstatic.com/"
        "shopping?q=tbn:ANd9GcQpn38jB2_BANnHUFa7kHJsf6SyubcgeU1lNYO_"
        "ZxM1Q2ju_ZMjv2EwNh0Zx_zbqYy_mFg_aiIhWYnD5PQ7t-uFzLM5cN77s_2_"
        "DFNeumI-LMPJMYjW-BOSaA&usqp=CAY");
    dummy_cart1->product_image_urls.emplace_back(
        "https://encrypted-tbn0.gstatic.com/"
        "shopping?q=tbn:ANd9GcQyMRYWeM2Yq095nOXTL0-"
        "EUUnm79kh6hnw8yctJUNrAuse607KEr1CVxEa24r-"
        "8XHBuhTwcuC4GXeN94h9Kn19DhdBGsXG0qrD74veYSDJNLrUP-sru0jH&usqp=CAY");
    dummy_cart1->product_image_urls.emplace_back(
        "https://encrypted-tbn1.gstatic.com/"
        "shopping?q=tbn:ANd9GcT2ew6Aydzu5VzRV756ORGha6fyjKp_On7iTlr_"
        "tL9vODnlNtFo_xsxj6_lCop-3J0Vk44lHfk-AxoBJDABVHPVFN-"
        "EiWLcZvzkdpHFqcurm7fBVmWtYKo2rg&usqp=CAY");
    carts.push_back(std::move(dummy_cart1));

    auto dummy_cart2 = chrome_cart::mojom::MerchantCart::New();
    dummy_cart2->merchant = "Cart Bar";
    dummy_cart2->cart_url = GURL("https://www.google.com/");
    carts.push_back(std::move(dummy_cart2));
  }
  std::move(callback).Run(std::move(carts));
}

void CartHandler::DismissCartModule() {
  CartServiceFactory::GetForProfile(profile_)->Dismiss();
}

void CartHandler::RestoreCartModule() {
  CartServiceFactory::GetForProfile(profile_)->Restore();
}
