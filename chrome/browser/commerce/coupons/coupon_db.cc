// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_db.h"

#include "chrome/browser/commerce/coupons/coupon_db_content.pb.h"
#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"

CouponDB::CouponDB(content::BrowserContext* browser_context)
    : proto_db_(
          ProfileProtoDBFactory<coupon_db::CouponContentProto>::GetInstance()
              ->GetForProfile(browser_context)) {}

CouponDB::~CouponDB() = default;
