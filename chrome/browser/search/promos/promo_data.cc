// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/promos/promo_data.h"

PromoData::PromoData() = default;
PromoData::PromoData(const PromoData&) = default;
PromoData::PromoData(PromoData&&) = default;
PromoData::~PromoData() = default;

PromoData& PromoData::operator=(const PromoData&) = default;
PromoData& PromoData::operator=(PromoData&&) = default;

bool operator==(const PromoData& lhs, const PromoData& rhs) {
  return lhs.promo_html == rhs.promo_html &&
         lhs.promo_log_url == rhs.promo_log_url && lhs.promo_id == rhs.promo_id;
}

bool operator!=(const PromoData& lhs, const PromoData& rhs) {
  return !(lhs == rhs);
}
