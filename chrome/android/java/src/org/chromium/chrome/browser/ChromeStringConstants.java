// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * Non-translatable strings cause apk size bloat if they are added in strings. Use constants
 * instead of marking the message 'translateable="false"'.
 */
public class ChromeStringConstants {
    public static final String AUTOFILL_MANAGE_WALLET_CARD_URL =
            "https://payments.google.com/#paymentMethods";
    public static final String AUTOFILL_MANAGE_PAYMENTS_CARDS_URL_FOR_GPAY_WEB =
            "https://pay.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods";
    public static final String AUTOFILL_MANAGE_WALLET_ADDRESSES_URL =
            "https://payments.google.com/#paymentMethods";
    public static final String SYNC_DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";
    // LINT.IfChange
    public static final String AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL =
            "https://support.google.com/googlepay/answer/11234179";
    // LINT.ThenChange(//components/autofill/core/browser/payments/payments_service_url.cc)
}
