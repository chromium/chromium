// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.components.signin.GAIAServiceType;

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

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void logProfileAccountManagementMenu(int metric, int gaiaServiceType);
    }

    private SigninMetricsUtils() {}
}
