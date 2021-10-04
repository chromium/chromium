// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COUPONS_COUPON_DB_H_
#define CHROME_BROWSER_COMMERCE_COUPONS_COUPON_DB_H_

#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace coupon_db {
class CouponContentProto;
}  // namespace coupon_db

template <typename T>
class ProfileProtoDB;

class CouponDB {
 public:
  explicit CouponDB(content::BrowserContext* browser_context);
  CouponDB(const CouponDB&) = delete;
  CouponDB& operator=(const CouponDB&) = delete;
  ~CouponDB();

 private:
  ProfileProtoDB<coupon_db::CouponContentProto>* proto_db_;
  base::WeakPtrFactory<CouponDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMMERCE_COUPONS_COUPON_DB_H_
