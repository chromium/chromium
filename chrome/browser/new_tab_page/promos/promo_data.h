// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_DATA_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_DATA_H_

#include <string>

#include "url/gurl.h"

// This struct contains all the data needed to inject a middle-slot Promo into
// a page.
struct PromoData {
  PromoData();
  PromoData(const PromoData&);
  PromoData(PromoData&&);
  ~PromoData();

  PromoData& operator=(const PromoData&);
  PromoData& operator=(PromoData&&);

  // The structured JSON data of the middle slot promo.
  std::string middle_slot_json;

  // URL to ping to log a promo impression. May be invalid.
  GURL promo_log_url;

  // The unique identifier for this promo. May be empty.
  std::string promo_id;

  // Allow the promo to open chrome://extensions for the extensions checkup
  // experiment.
  bool can_open_extensions_page = false;
};

bool operator==(const PromoData& lhs, const PromoData& rhs);
bool operator!=(const PromoData& lhs, const PromoData& rhs);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_DATA_H_
