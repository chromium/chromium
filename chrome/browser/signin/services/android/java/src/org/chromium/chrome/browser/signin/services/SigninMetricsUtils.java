// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;

/**
 * Util methods for signin metrics logging.
 */
public class SigninMetricsUtils {
    /**
     * Logs a {@link ProfileAccountManagementMetrics} for a given {@link GAIAServiceType}.
     */
    public static void logProfileAccountManagementMenu(
            @ProfileAccountManagementMetrics int metric, @GAIAServiceType int gaiaServiceType) {
        SigninMetricsUtilsJni.get().logProfileAccountManagementMenu(metric, gaiaServiceType);
    }

    /**
     * Logs Signin.AccountConsistencyPromoAction histogram.
     */
    public static void logAccountConsistencyPromoAction(
            @AccountConsistencyPromoAction int promoAction) {
        RecordHistogram.recordEnumeratedHistogram("Signin.AccountConsistencyPromoAction",
                promoAction, AccountConsistencyPromoAction.MAX);
    }

    /**
     * Logs AccountPickerBottomSheet shown count histograms.
     */
    public static void logAccountConsistencyPromoShownCount(String histogram) {
        RecordHistogram.recordExactLinearHistogram(histogram,
                SigninPreferencesManager.getInstance().getAccountPickerBottomSheetShownCount(),
                100);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void logProfileAccountManagementMenu(int metric, int gaiaServiceType);
    }

    private SigninMetricsUtils() {}
}
