// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import org.chromium.build.annotations.NullMarked;

/** Constants used across Autofill payment method settings. */
@NullMarked
/*package*/ final class AutofillPaymentMethodsConstants {
    // Google Wallet URL used in the reminder notice preference.
    // LINT.IfChange(WALLET_REMINDER_NOTICE_URLS)
    static final String WALLET_SETTINGS_URL =
            "https://wallet.google.com/wallet?p=settings&utm_source=chrome&utm_medium=settings&utm_campaign=settings";

    // LINT.ThenChange(//components/autofill/core/browser/payments/payments_service_url.cc:WALLET_REMINDER_NOTICE_URLS)

    private AutofillPaymentMethodsConstants() {}
}
