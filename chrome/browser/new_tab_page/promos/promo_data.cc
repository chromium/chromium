// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/promos/promo_data.h"

PromoData::PromoData() = default;
PromoData::PromoData(const PromoData&) = default;
PromoData::PromoData(PromoData&&) = default;
PromoData::~PromoData() = default;

PromoData& PromoData::operator=(const PromoData&) = default;
PromoData& PromoData::operator=(PromoData&&) = default;

bool operator==(const PromoData& lhs, const PromoData& rhs) {
  return lhs.middle_slot_json == rhs.middle_slot_json &&
         lhs.promo_log_url == rhs.promo_log_url && lhs.promo_id == rhs.promo_id;
}

bool operator!=(const PromoData& lhs, const PromoData& rhs) {
  return !(lhs == rhs);
}
