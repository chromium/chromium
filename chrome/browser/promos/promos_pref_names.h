// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROMOS_PROMOS_PREF_NAMES_H_
#define CHROME_BROWSER_PROMOS_PROMOS_PREF_NAMES_H_

namespace promos_prefs {

// Pref names for the "Desktop to iOS" promos where users are offered a QR code
// on Desktop they can scan to download the mobile app.

// iOS Password promo prefs.
// Int64 that keeps track of the last impression timestamp of the "iOS password
// promo bubble on desktop" for a given user.
inline constexpr char kDesktopToiOSPasswordPromoLastImpressionTimestamp[] =
    "promos.ios_password_last_impression_timestamp";

// Integer that keeps track of impressions of the "iOS password promo bubble on
// desktop" shown to a given user.
inline constexpr char kDesktopToiOSPasswordPromoImpressionsCounter[] =
    "promos.ios_password_impressions_counter";

// Boolean that keeps track whether a given user has opted-out of seeing the
// "iOS password promo bubble on desktop" again.
inline constexpr char kDesktopToiOSPasswordPromoOptOut[] =
    "promos.ios_password_opt_out";

// iOS Address promo prefs.
// Int64 that keeps track of the last impression timestamp of the "iOS address
// promo bubble on desktop" for a given user.
inline constexpr char kDesktopToiOSAddressPromoLastImpressionTimestamp[] =
    "promos.ios_address_last_impression_timestamp";

// Integer that keeps track of impressions of the "iOS address promo bubble on
// desktop" shown to a given user.
inline constexpr char kDesktopToiOSAddressPromoImpressionsCounter[] =
    "promos.ios_address_impressions_counter";

// Boolean that keeps track whether a given user has opted-out of seeing the
// "iOS address promo bubble on desktop" again.
inline constexpr char kDesktopToiOSAddressPromoOptOut[] =
    "promos.ios_address_opt_out";

// iOS Payment promo prefs.
// Int64 that keeps track of the last impression timestamp of the "iOS payment
// promo bubble on desktop" for a given user.
inline constexpr char kDesktopToiOSPaymentPromoLastImpressionTimestamp[] =
    "promos.ios_payment_last_impression_timestamp";

// Integer that keeps track of impressions of the "iOS payment promo bubble on
// desktop" shown to a given user.
inline constexpr char kDesktopToiOSPaymentPromoImpressionsCounter[] =
    "promos.ios_payment_impressions_counter";

// Boolean that keeps track whether a given user has opted-out of seeing the
// "iOS payment promo bubble on desktop" again.
inline constexpr char kDesktopToiOSPaymentPromoOptOut[] =
    "promos.ios_payment_opt_out";

}  // namespace promos_prefs

#endif  // CHROME_BROWSER_PROMOS_PROMOS_PREF_NAMES_H_
