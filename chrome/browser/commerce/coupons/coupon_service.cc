// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_service.h"

#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"

namespace {

void ConstructCouponProto(
    const GURL& origin,
    const std::vector<std::unique_ptr<autofill::AutofillOfferData>>& offers,
    const CouponService::CouponDisplayTimeMap& coupon_time_map,
    coupon_db::CouponContentProto* proto) {
  proto->set_key(origin.spec());
  for (const auto& offer : offers) {
    coupon_db::FreeListingCouponInfoProto* coupon_info_proto =
        proto->add_free_listing_coupons();
    coupon_info_proto->set_coupon_description(
        offer->GetDisplayStrings().value_prop_text);
    coupon_info_proto->set_coupon_code(offer->GetPromoCode());
    coupon_info_proto->set_coupon_id(offer->GetOfferId());
    coupon_info_proto->set_expiry_time(
        offer->GetExpiry().InSecondsFSinceUnixEpoch());
    std::pair<GURL, int64_t> key({origin, offer->GetOfferId()});
    if (coupon_time_map.find(key) != coupon_time_map.end()) {
      coupon_info_proto->set_last_display_time(
          coupon_time_map.at(key).InMillisecondsSinceUnixEpoch());
    } else {
      // Unknown last display time; set to zero so the reminder bubble will
      // always appear.
      coupon_info_proto->set_last_display_time(
          base::Time().InMillisecondsSinceUnixEpoch());
    }
  }
}

bool CompareCouponList(
    const std::vector<std::unique_ptr<autofill::AutofillOfferData>>&
        coupon_list_a,
    const std::vector<std::unique_ptr<autofill::AutofillOfferData>>&
        coupon_list_b) {
  return base::ranges::equal(
      coupon_list_a, coupon_list_b, std::equal_to<>(),
      &std::unique_ptr<autofill::AutofillOfferData>::operator*,
      &std::unique_ptr<autofill::AutofillOfferData>::operator*);
}

}  // namespace

CouponService::CouponService(std::unique_ptr<CouponDB> coupon_db)
    : coupon_db_(std::move(coupon_db)) {
  InitializeCouponsMap();
}
CouponService::~CouponService() = default;

void CouponService::UpdateFreeListingCoupons(const CouponsMap& coupon_map) {
  if (!features_enabled_)
    return;
  // Identify origins whose coupon has changed in the new data.
  std::vector<GURL> invalid_coupon_origins;
  for (const auto& entry : coupon_map_) {
    const GURL& origin = entry.first;
    if (!coupon_map.contains(origin) ||
        !CompareCouponList(coupon_map.at(origin), coupon_map_.at(origin))) {
      invalid_coupon_origins.emplace_back(origin);
    }
  }
  for (const GURL& origin : invalid_coupon_origins) {
    NotifyObserversOfInvalidatedCoupon(origin);
    coupon_map_.erase(origin);
  }
  coupon_db_->DeleteAllCoupons();
  CouponDisplayTimeMap new_time_map;
  for (const auto& entry : coupon_map) {
    const GURL& origin(entry.first.DeprecatedGetOriginAsURL());
    for (const auto& coupon : entry.second) {
      if (!coupon_map_.contains(origin)) {
        auto new_coupon =
            std::make_unique<autofill::AutofillOfferData>(*coupon);
        coupon_map_[origin].emplace_back(std::move(new_coupon));
      }
      new_time_map[{origin, coupon->GetOfferId()}] =
          coupon_time_map_[{origin, coupon->GetOfferId()}];
    }
    coupon_db::CouponContentProto proto;
    ConstructCouponProto(origin, entry.second, coupon_time_map_, &proto);
    coupon_db_->AddCoupon(origin, proto);
  }
  coupon_time_map_ = new_time_map;
}

void CouponService::DeleteFreeListingCouponsForUrl(const GURL& url) {
  if (!url.is_valid())
    return;
  const GURL& origin(url.DeprecatedGetOriginAsURL());
  NotifyObserversOfInvalidatedCoupon(origin);
  coupon_map_.erase(origin);
  coupon_db_->DeleteCoupon(origin);
}

void CouponService::DeleteAllFreeListingCoupons() {
  for (const auto& entry : coupon_map_) {
    NotifyObserversOfInvalidatedCoupon(entry.first);
  }
  coupon_map_.clear();
  coupon_db_->DeleteAllCoupons();
}

base::Time CouponService::GetCouponDisplayTimestamp(
    const autofill::AutofillOfferData& offer) {
  for (auto origin : offer.GetMerchantOrigins()) {
    auto iter =
        coupon_time_map_.find(std::make_pair(origin, offer.GetOfferId()));
    if (iter != coupon_time_map_.end())
      return iter->second;
  }
  return base::Time();
}

void CouponService::RecordCouponDisplayTimestamp(
    const autofill::AutofillOfferData& offer) {
  base::Time timestamp = base::Time::Now();
  for (auto origin : offer.GetMerchantOrigins()) {
    auto iter =
        coupon_time_map_.find(std::make_pair(origin, offer.GetOfferId()));
    if (iter != coupon_time_map_.end()) {
      iter->second = timestamp;
      coupon_db_->LoadCoupon(
          origin, base::BindOnce(&CouponService::OnUpdateCouponTimestamp,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 offer.GetOfferId(), timestamp));
    }
  }
}

void CouponService::MaybeFeatureStatusChanged(bool enabled) {
  enabled &=
      (commerce::IsCouponWithCodeEnabled() || commerce::IsFakeDataEnabled());
  if (enabled == features_enabled_)
    return;
  features_enabled_ = enabled;
  if (!enabled)
    DeleteAllFreeListingCoupons();
}

CouponService::Coupons CouponService::GetFreeListingCouponsForUrl(
    const GURL& url) {
  const GURL& origin(url.DeprecatedGetOriginAsURL());
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
  return coupon_map_.find(url.DeprecatedGetOriginAsURL()) != coupon_map_.end();
}

void CouponService::AddObserver(CouponServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void CouponService::RemoveObserver(CouponServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

CouponService::CouponService() = default;

void CouponService::InitializeCouponsMap() {
  coupon_db_->LoadAllCoupons(base::BindOnce(
      &CouponService::OnInitializeCouponsMap, weak_ptr_factory_.GetWeakPtr()));
}

void CouponService::OnInitializeCouponsMap(
    bool success,
    std::vector<CouponDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  for (auto pair : proto_pairs) {
    const GURL origin(GURL(pair.first));
    for (auto coupon : pair.second.free_listing_coupons()) {
      int64_t offer_id = coupon.coupon_id();
      base::Time expiry =
          base::Time::FromSecondsSinceUnixEpoch(coupon.expiry_time());
      std::vector<GURL> merchant_origins;
      merchant_origins.emplace_back(origin);
      GURL offer_details_url = GURL();
      autofill::DisplayStrings display_strings;
      display_strings.value_prop_text = coupon.coupon_description();
      std::string promo_code = coupon.coupon_code();

      auto offer = std::make_unique<autofill::AutofillOfferData>(
          autofill::AutofillOfferData::FreeListingCouponOffer(
              offer_id, expiry, merchant_origins, offer_details_url,
              display_strings, promo_code));
      coupon_map_[origin].emplace_back(std::move(offer));
      coupon_time_map_[{origin, coupon.coupon_id()}] =
          base::Time::FromMillisecondsSinceUnixEpoch(
              coupon.last_display_time());
    }
  }
}

void CouponService::OnUpdateCouponTimestamp(
    int64_t coupon_id,
    const base::Time last_display_timestamp,
    bool success,
    std::vector<CouponDB::KeyAndValue> proto_pairs) {
  if (!success) {
    return;
  }
  if (proto_pairs.empty())
    return;
  coupon_db::CouponContentProto proto = proto_pairs[0].second;
  for (int i = 0; i < proto.free_listing_coupons_size(); ++i) {
    if (proto.free_listing_coupons()[i].coupon_id() != coupon_id)
      continue;
    coupon_db::FreeListingCouponInfoProto* coupon_proto =
        proto.mutable_free_listing_coupons(i);
    coupon_proto->set_last_display_time(
        last_display_timestamp.InMillisecondsSinceUnixEpoch());
    coupon_db_->AddCoupon(GURL(proto_pairs[0].first), proto);
    return;
  }
}

CouponDB* CouponService::GetDB() {
  return coupon_db_.get();
}

void CouponService::NotifyObserversOfInvalidatedCoupon(const GURL& url) {
  for (const auto& offer : coupon_map_[url]) {
    for (CouponServiceObserver& observer : observers_) {
      observer.OnCouponInvalidated(*offer);
    }
  }
}
