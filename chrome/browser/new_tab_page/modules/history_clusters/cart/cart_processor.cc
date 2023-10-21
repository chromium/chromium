// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "components/search/ntp_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/time_format.h"

namespace {
constexpr char kGoogleDomain[] = "google.com";
constexpr char kGoogleStoreHost[] = "store.google.com";

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
      base::Time::Now() -
          base::Time::FromSecondsSinceUnixEpoch(cart.second.timestamp())));
  cart_mojom->discount_text = cart.second.discount_info().discount_text();
  return cart_mojom;
}

ntp::history_clusters::cart::mojom::CartPtr GenerateSampleCart(int imageCount) {
  auto cart_mojom = ntp::history_clusters::cart::mojom::Cart::New();
  cart_mojom->domain = "google.com";
  cart_mojom->merchant = "Google Store";
  cart_mojom->cart_url = GURL("https://store.google.com/cart");
  cart_mojom->relative_date = "2 days ago";
  cart_mojom->discount_text = "15% off";
  const std::string image_url =
      "https://lh3.googleusercontent.com/"
      "uGKKV2OSGGGu51DDu5qByS-EmYzTpH2sqoRzY7vG4sco_"
      "5bRjS8lHpn0NaMbe7EhVIBv2bfVQpUlb50lfRzKiXM7Y_yybph9-qE=s0";
  for (int i = 0; i < imageCount; i++) {
    cart_mojom->product_image_urls.emplace_back(image_url);
  }
  return cart_mojom;
}
}  // namespace

CartProcessor::CartProcessor(CartService* cart_service)
    : cart_service_(cart_service) {}

CartProcessor::~CartProcessor() = default;

bool CartProcessor::IsCartAssociatedWithVisitURL(CartDB::KeyAndValue& cart,
                                                 GURL visit_url) {
  if (cart.first == kGoogleDomain && visit_url.host() != kGoogleStoreHost) {
    return false;
  }
  return cart.first == eTLDPlusOne(visit_url);
}

void CartProcessor::GetCartForCluster(
    history_clusters::mojom::ClusterPtr cluster,
    ntp::history_clusters::mojom::PageHandler::GetCartForClusterCallback
        callback) {
  if (!cart_service_->IsCartEnabled()) {
    std::move(callback).Run(nullptr);
    return;
  }
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpChromeCartInHistoryClusterModule,
      ntp_features::kNtpChromeCartInHistoryClustersModuleDataParam);
  if (!fake_data_param.empty()) {
    int image_count;
    if (!base::StringToInt(fake_data_param, &image_count)) {
      return;
    }
    std::move(callback).Run(GenerateSampleCart(image_count));
    return;
  }
  cart_service_->LoadAllActiveCarts(
      base::BindOnce(&CartProcessor::OnLoadCart, weak_ptr_factory_.GetWeakPtr(),
                     std::move(cluster), std::move(callback)));
}

void CartProcessor::RecordCartHistoryClusterAssociationMetrics(
    std::vector<CartDB::KeyAndValue>& active_carts,
    std::vector<history::Cluster>& clusters) {
  for (auto cart_pair : active_carts) {
    bool match_cluster = false;
    for (size_t i = 0; i < clusters.size(); i++) {
      for (auto visit : clusters[i].visits) {
        if (IsCartAssociatedWithVisitURL(cart_pair, visit.normalized_url)) {
          match_cluster = true;
          break;
        }
      }
      if (match_cluster) {
        commerce::CartHistoryClusterAssociationStatus status =
            (i == 0 ? commerce::CartHistoryClusterAssociationStatus::
                          kAssociatedWithTopCluster
                    : commerce::CartHistoryClusterAssociationStatus::
                          kAssociatedWithNonTopCluster);
        base::UmaHistogramEnumeration(
            "NewTabPage.HistoryClusters.CartAssociationStatus", status);
        break;
      }
    }
    if (!match_cluster) {
      base::UmaHistogramEnumeration(
          "NewTabPage.HistoryClusters.CartAssociationStatus",
          commerce::CartHistoryClusterAssociationStatus::
              kNotAssociatedWithCluster);
    }
  }
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
      if (IsCartAssociatedWithVisitURL(cart, visit->normalized_url)) {
        std::move(callback).Run(CartToMojom(cart));
        return;
      }
    }
  }
  std::move(callback).Run(nullptr);
}
