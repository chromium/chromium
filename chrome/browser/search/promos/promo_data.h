// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_PROMOS_PROMO_DATA_H_
#define CHROME_BROWSER_SEARCH_PROMOS_PROMO_DATA_H_

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

  // The main HTML for the promo. May be empty when nothing to show.
  std::string promo_html;

  // URL to ping to log a promo impression. May be invalid.
  GURL promo_log_url;

  // The unique identifier for this promo. May be empty.
  std::string promo_id;

  // Allow the promo to open chrome:// links
  bool can_open_privileged_links = false;
};

bool operator==(const PromoData& lhs, const PromoData& rhs);
bool operator!=(const PromoData& lhs, const PromoData& rhs);

#endif  // CHROME_BROWSER_SEARCH_PROMOS_PROMO_DATA_H_
