// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/time_format.h"

namespace {
std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

ntp::history_clusters::cart::mojom::CartPtr CartToMojom(
    CartDB::KeyAndValue cart) {
  auto cart_mojom = ntp::history_clusters::cart::mojom::Cart::New();
  cart_mojom->domain = cart.first;
  cart_mojom->merchant = cart.second.merchant();
  cart_mojom->cart_url = GURL(cart.second.merchant_cart_url());
  for (std::string image_url : cart.second.product_image_urls()) {
    cart_mojom->product_image_urls.emplace_back(std::move(image_url));
  }
  cart_mojom->relative_date = base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
      base::Time::Now() - base::Time::FromDoubleT(cart.second.timestamp())));
  return cart_mojom;
}
}  // namespace

CartProcessor::CartProcessor(CartService* cart_service)
    : cart_service_(cart_service) {}

CartProcessor::~CartProcessor() = default;

void CartProcessor::GetCartForCluster(
    history_clusters::mojom::ClusterPtr cluster,
    ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback
        callback) {
  cart_service_->LoadAllActiveCarts(
      base::BindOnce(&CartProcessor::OnLoadCart, weak_ptr_factory_.GetWeakPtr(),
                     std::move(cluster), std::move(callback)));
}

void CartProcessor::OnLoadCart(
    history_clusters::mojom::ClusterPtr cluster,
    ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback
        callback,
    bool success,
    std::vector<CartDB::KeyAndValue> carts) {
  if (!success) {
    std::move(callback).Run(nullptr);
    return;
  }
  for (auto cart : carts) {
    for (auto& visit : cluster->visits) {
      if (cart.first == eTLDPlusOne(GURL(visit->url_for_display))) {
        std::move(callback).Run(CartToMojom(cart));
        return;
      }
    }
  }
  std::move(callback).Run(nullptr);
}
