// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/commerce/coupons/coupon_db_content.pb.h"

namespace {

void ConstructCouponProto(
    coupon_db::CouponContentProto* proto,
    const GURL& origin,
    const std::vector<std::unique_ptr<autofill::AutofillOfferData>>& offers) {
  proto->set_key(origin.spec());
  for (const auto& offer : offers) {
    coupon_db::FreeListingCouponInfoProto* coupon_info_proto =
        proto->add_free_listing_coupons();
    coupon_info_proto->set_coupon_description(
        offer->display_strings.value_prop_text);
    coupon_info_proto->set_coupon_code(offer->promo_code);
    coupon_info_proto->set_coupon_id(offer->offer_id);
    coupon_info_proto->set_expiry_time(offer->expiry.ToDoubleT());
  }
}

}  // namespace

CouponService::CouponService(std::unique_ptr<CouponDB> coupon_db)
    : coupon_db_(std::move(coupon_db)) {
  InitializeCouponsMap();
}
CouponService::~CouponService() = default;
CouponService::CouponService() = default;

void CouponService::UpdateFreeListingCoupons(const CouponsMap& coupon_map) {
  coupon_db_->DeleteAllCoupons();
  coupon_map_.clear();
  for (const auto& entry : coupon_map) {
    const GURL& origin(entry.first.GetOrigin());
    for (const auto& coupon : entry.second) {
      coupon_map_[origin].emplace_back(
          std::make_unique<autofill::AutofillOfferData>(*coupon));
    }
    coupon_db::CouponContentProto proto;
    ConstructCouponProto(&proto, origin, entry.second);
    coupon_db_->AddCoupon(origin, proto);
  }
}

void CouponService::DeleteFreeListingCouponsForUrl(const GURL& url) {
  if (!url.is_valid())
    return;
  const GURL& origin(url.GetOrigin());
  coupon_map_.erase(origin);
  coupon_db_->DeleteCoupon(origin);
}

void CouponService::DeleteAllFreeListingCoupons() {
  coupon_map_.clear();
  coupon_db_->DeleteAllCoupons();
}

CouponService::Coupons CouponService::GetFreeListingCouponsForUrl(
    const GURL& url) {
  if (!url.is_valid())
    return {};
  const GURL& origin(url.GetOrigin());
  if (coupon_map_.find(origin) == coupon_map_.end()) {
    return {};
  }
  Coupons result;
  result.reserve(coupon_map_.at(origin).size());
  for (const auto& data : coupon_map_.at(origin))
    result.emplace_back(data.get());
  return result;
}

bool CouponService::IsUrlEligible(const GURL& url) {
  if (!url.is_valid())
    return false;
  return coupon_map_.find(url.GetOrigin()) != coupon_map_.end();
}

CouponDB* CouponService::GetDB() {
  return coupon_db_.get();
}

void CouponService::InitializeCouponsMap() {
  coupon_db_->LoadAllCoupons(base::BindOnce(
      &CouponService::OnInitializeCouponsMap, weak_ptr_factory_.GetWeakPtr()));
}

void CouponService::OnInitializeCouponsMap(
    bool success,
    std::vector<CouponDB::KeyAndValue> proto_pairs) {
  DCHECK(success);
  for (auto pair : proto_pairs) {
    const GURL origin(GURL(pair.first));
    for (auto coupon : pair.second.free_listing_coupons()) {
      auto offer = std::make_unique<autofill::AutofillOfferData>();
      offer->display_strings.value_prop_text = coupon.coupon_description();
      offer->promo_code = coupon.coupon_code();
      offer->merchant_origins.emplace_back(origin);
      offer->offer_id = coupon.coupon_id();
      offer->expiry = base::Time::FromDoubleT(coupon.expiry_time());
      coupon_map_[origin].emplace_back(std::move(offer));
    }
  }
}
