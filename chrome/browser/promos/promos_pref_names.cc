// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_pref_names.h"

namespace promos_prefs {
// Int64 that keeps track of the last impression timestamp of the "iOS password
// promo bubble on desktop" for a given user.
const char kiOSPasswordPromoLastImpressionTimestamp[] =
    "promos.ios_password_last_impression_timestamp";
// Integer that keeps track of impressions of the "iOS password promo bubble on
// desktop" shown to a given user.
const char kiOSPasswordPromoImpressionsCounter[] =
    "promos.ios_password_impressions_counter";
// Boolean that keeps track whether a given user has opted-out of seeing the
// "iOS password promo bubble on desktop" again.
const char kiOSPasswordPromoOptOut[] = "promos.ios_password_opt_out";
}  // namespace promos_prefs
