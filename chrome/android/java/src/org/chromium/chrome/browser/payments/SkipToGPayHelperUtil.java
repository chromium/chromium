// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.collection.ArrayMap;

import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.SkipToGPayHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.Map;

/**
 * This class contains all static utility methods for {@link SkipToGPayHelper}. This class should be
 * combined into {@link SkipToGPayHelper} once all browser dependencies (e.g., {@link
 * AutofillPaymentAppFactory}) are removed.
 */
public final class SkipToGPayHelperUtil {
    /**
     * Returns whether the skip-to-GPay experiment should be enabled.
     * @param webContents The WebContents that triggered the PaymentRequest.
     * @param rawMethodData The PaymentMethodData[] provided to PaymentRequest constructor.
     * @return True if either of the two skip-to-GPay experiment flow can be enabled.
     */
    public static boolean canActivateExperiment(
            WebContents webContents, PaymentMethodData[] rawMethodData) {
        if (rawMethodData == null || rawMethodData.length == 0) return false;

        Map<String, PaymentMethodData> methodData = new ArrayMap<>();
        for (int i = 0; i < rawMethodData.length; i++) {
            String method = rawMethodData[i].supportedMethod;
            if (method.equals(MethodStrings.BASIC_CARD)) {
                methodData.put(method, rawMethodData[i]);
                break;
            }
        }
        if (methodData.isEmpty()) return false;

        // V2 experiment: enable skip-to-GPay regardless of usable basic-card.
        if (PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                    PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY)) {
            return true;
        }

        // V1 experiment: only enable skip-to-GPay if no usable basic-card exists.
        // This check for autofill card is duplicate work if skip-to-GPay ends up not being
        // enabled and adds a small delay (average ~3ms with first time ) to all hybrid request
        // flows. However, this is the cleanest way to implement SKIP_TO_GPAY_IF_NO_CARD.
        return !AutofillPaymentAppFactory.hasUsableAutofillCard(webContents, methodData)
                && PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                        PaymentFeatureList.PAYMENT_REQUEST_SKIP_TO_GPAY_IF_NO_CARD);
    }
}
