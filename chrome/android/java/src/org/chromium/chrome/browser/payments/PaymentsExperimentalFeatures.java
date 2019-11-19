// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Utility wrapper around |ChromeFeatureList| to enable kWebPaymentsExperimentalFeature to be used
 * as an override to all payments features.
 */
public final class PaymentsExperimentalFeatures {
    /**
     * Returns whether the specified feature is enabled or not.
     *
     * This API differs from |ChromeFeatureList| in that it also returns true if the
     * |kWebPaymentsExperimentalFeatures| feature is enabled.
     */
    public static boolean isEnabled(String featureName) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_PAYMENTS_EXPERIMENTAL_FEATURES)
                || ChromeFeatureList.isEnabled(featureName);
    }

    private PaymentsExperimentalFeatures() {}
}
