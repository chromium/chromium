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
import org.chromium.components.signin.metrics.SigninAccessPoint;

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
     * Logs the access point when the user see the view of choosing account to sign in. Sign-in
     * completion histogram is recorded by {@link SigninManager#signinAndEnableSync}.
     *
     * @param accessPoint {@link SigninAccessPoint} that initiated the sign-in flow.
     */
    public static void logSigninStartAccessPoint(@SigninAccessPoint int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninStartedAccessPoint", accessPoint, SigninAccessPoint.MAX);
    }

    /**
     * Logs signin user action for a given {@link SigninAccessPoint}.
     */
    public static void logSigninUserActionForAccessPoint(@SigninAccessPoint int accessPoint) {
        SigninMetricsUtilsJni.get().logSigninUserActionForAccessPoint(accessPoint);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void logProfileAccountManagementMenu(int metric, int gaiaServiceType);
        void logSigninUserActionForAccessPoint(int accessPoint);
    }

    private SigninMetricsUtils() {}
}
