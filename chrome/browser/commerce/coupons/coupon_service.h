// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_H_
#define CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/commerce/coupons/coupon_db.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/commerce/coupons/coupon_service_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/keyed_service/core/keyed_service.h"

// Service to host coupon-related logics.
class CouponService : public KeyedService {
 public:
  // TODO(b/351080010): Migrate CouponService to use DiscountInfo instead of
  // AutofillOfferData.
  using Coupons = std::vector<autofill::AutofillOfferData*>;
  using CouponsMap =
      base::flat_map<GURL,
                     std::vector<std::unique_ptr<autofill::AutofillOfferData>>>;
  // Key is a pair of the origin of the merchant that a coupon belongs to and
  // the coupon ID.
  using CouponDisplayTimeMap =
      base::flat_map<std::pair<GURL, int64_t>, base::Time>;

  // Use |CouponServiceFactory::GetForProfile(...)| to get an instance of this
  // service.
  explicit CouponService(std::unique_ptr<CouponDB> coupon_db);
  CouponService(const CouponService&) = delete;
  CouponService& operator=(const CouponService&) = delete;
  ~CouponService() override;

  // Update coupon data both in cache layer and storage to make sure it reflects
  // the latest status of fetching from server.
  void UpdateFreeListingCoupons(const CouponsMap& coupons_map);

  // Delete the FreeListing coupon for the given URL in the cache layer and
  // storage.
  virtual void DeleteFreeListingCouponsForUrl(const GURL& url);

  // Delete all the Freelisting coupons in the cache layer and storage.
  virtual void DeleteAllFreeListingCoupons();

  // Get the last time that |offer| has shown in infobar bubble.
  virtual base::Time GetCouponDisplayTimestamp(
      const autofill::AutofillOfferData& offer);

  // Record the last display timestamp of a coupon in the cache layer and
  // storage.
  virtual void RecordCouponDisplayTimestamp(
      const autofill::AutofillOfferData& offer);

  // Get called when cart or discount feature status might have changed.
  virtual void MaybeFeatureStatusChanged(bool enabled);

  // Get FreeListing coupons for the given URL. Will return an empty
  // list if there is no coupon data associated with this URL.
  Coupons GetFreeListingCouponsForUrl(const GURL& url);

  // Check if CouponService has eligible coupons for |url|.
  bool IsUrlEligible(const GURL& url);

  void AddObserver(CouponServiceObserver* observer);

  void RemoveObserver(CouponServiceObserver* observer);

 protected:
  // Default constructor for testing purposes only.
  CouponService();

 private:
  friend class CouponServiceFactory;
  friend class CouponServiceTest;
  friend class CartServiceCouponTest;

  // Initialize the coupon map in cache layer from storage.
  void InitializeCouponsMap();

  // Callback to initialize the coupon map.
  void OnInitializeCouponsMap(bool success,
                              std::vector<CouponDB::KeyAndValue> proto_pairs);

  // Callback to update coupon last display timestamp.
  void OnUpdateCouponTimestamp(int64_t coupon_id,
                               base::Time timestamp,
                               bool success,
                               std::vector<CouponDB::KeyAndValue> proto_pairs);
  CouponDB* GetDB();
  // Dispatch signals to registered CouponServiceObservers that the coupons for
  // |url| are no longer valid. Note that this call should be made before the
  // coupons are deleted from cache.
  void NotifyObserversOfInvalidatedCoupon(const GURL& url);

  std::unique_ptr<CouponDB> coupon_db_;
  CouponsMap coupon_map_;
  CouponDisplayTimeMap coupon_time_map_;
  // Indicates whether features required for CouponService to expose coupon data
  // are all enabled.
  bool features_enabled_{false};
  base::ObserverList<CouponServiceObserver> observers_;
  base::WeakPtrFactory<CouponService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_H_
