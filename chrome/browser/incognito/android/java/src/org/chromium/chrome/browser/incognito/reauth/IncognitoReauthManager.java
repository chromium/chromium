// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * This class is responsible for managing the reauthentication flow when the Incognito session is
 * locked.
 */
public class IncognitoReauthManager {
    private static Boolean sIsIncognitoReauthFeatureAvailableForTesting;

    /**
     * @return A boolean indicating if the Incognito re-authentication feature is available.
     */
    public static boolean isIncognitoReauthFeatureAvailable() {
        if (sIsIncognitoReauthFeatureAvailableForTesting != null) {
            return sIsIncognitoReauthFeatureAvailableForTesting;
        }
        // The current phase relies on using the {@link BiometricManager} API which was added in
        // Android Version 29.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)
                && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q);
    }

    @VisibleForTesting
    public static void setIsIncognitoReauthFeatureAvailableForTesting(boolean isAvailable) {
        sIsIncognitoReauthFeatureAvailableForTesting = isAvailable;
    }
}
