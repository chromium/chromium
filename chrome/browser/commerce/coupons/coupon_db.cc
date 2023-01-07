// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_db.h"

#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"

CouponDB::CouponDB(content::BrowserContext* browser_context)
    : proto_db_(
          SessionProtoDBFactory<coupon_db::CouponContentProto>::GetInstance()
              ->GetForProfile(browser_context)) {}

CouponDB::~CouponDB() = default;

void CouponDB::LoadCoupon(const GURL& origin, LoadCallback callback) {
  DCHECK_EQ(origin.DeprecatedGetOriginAsURL(), origin);
  proto_db_->LoadOneEntry(origin.spec(), std::move(callback));
}

void CouponDB::LoadAllCoupons(LoadCallback callback) {
  proto_db_->LoadAllEntries(std::move(callback));
}

void CouponDB::AddCoupon(const GURL& origin,
                         const coupon_db::CouponContentProto& proto) {
  DCHECK_EQ(origin.DeprecatedGetOriginAsURL(), origin);
  proto_db_->InsertContent(origin.spec(), proto,
                           base::BindOnce(&CouponDB::OnOperationFinished,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void CouponDB::DeleteCoupon(const GURL& origin) {
  DCHECK_EQ(origin.DeprecatedGetOriginAsURL(), origin);
  proto_db_->DeleteOneEntry(origin.spec(),
                            base::BindOnce(&CouponDB::OnOperationFinished,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void CouponDB::DeleteAllCoupons() {
  proto_db_->DeleteAllContent(base::BindOnce(&CouponDB::OnOperationFinished,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void CouponDB::OnOperationFinished(bool success) {
  DCHECK(success) << "database operation failed.";
}
