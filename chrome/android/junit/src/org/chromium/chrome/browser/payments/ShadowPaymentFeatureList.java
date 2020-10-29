// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.components.payments.PaymentFeatureList;

/** The shadow of PaymentFeatureList. */
@Implements(PaymentFeatureList.class)
public class ShadowPaymentFeatureList {
    private static boolean sIsWebPaymentFeatureEnabled;

    /**
     * Set the {@link PaymentFeatureList.WEB_PAYMENTS} feature to be enabled.
     * @param enabled Whether to enable the feature.
     */
    public static void setWebPaymentsFeatureEnabled(boolean enabled) {
        sIsWebPaymentFeatureEnabled = enabled;
    }

    @Resetter
    public static void reset() {
        sIsWebPaymentFeatureEnabled = false;
    }

    @Implementation
    public static boolean isEnabled(String featureName) {
        if (featureName.equals(PaymentFeatureList.WEB_PAYMENTS)) {
            return sIsWebPaymentFeatureEnabled;
        }
        return false;
    }
}