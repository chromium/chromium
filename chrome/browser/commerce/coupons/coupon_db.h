// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COUPONS_COUPON_DB_H_
#define CHROME_BROWSER_COMMERCE_COUPONS_COUPON_DB_H_

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace coupon_db {
class CouponContentProto;
}  // namespace coupon_db

template <typename T>
class SessionProtoDB;

class CouponDB {
 public:
  using KeyAndValue = std::pair<std::string, coupon_db::CouponContentProto>;

  // Callback which is used when coupons are acquired.
  using LoadCallback = base::OnceCallback<void(bool, std::vector<KeyAndValue>)>;

  // Used for confirming an operation was completed successfully (e.g.
  // insert, delete).
  using OperationCallback = base::OnceCallback<void(bool)>;

  explicit CouponDB(content::BrowserContext* browser_context);
  CouponDB(const CouponDB&) = delete;
  CouponDB& operator=(const CouponDB&) = delete;
  ~CouponDB();

  // Load the coupon entry for a URL origin.
  void LoadCoupon(const GURL& origin, LoadCallback callback);

  // Load all the coupon entries;
  void LoadAllCoupons(LoadCallback callback);

  // Add a coupon entry to the database.
  void AddCoupon(const GURL& origin,
                 const coupon_db::CouponContentProto& proto);

  // Delete the coupon entry associated with certain origin in the database.
  void DeleteCoupon(const GURL& origin);

  // Delete all coupon entries in the database.
  void DeleteAllCoupons();

  // Callback when a database operation (e.g. insert or delete) is finished.
  void OnOperationFinished(bool success);

 private:
  raw_ptr<SessionProtoDB<coupon_db::CouponContentProto>,
          AcrossTasksDanglingUntriaged>
      proto_db_;
  base::WeakPtrFactory<CouponDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMMERCE_COUPONS_COUPON_DB_H_
