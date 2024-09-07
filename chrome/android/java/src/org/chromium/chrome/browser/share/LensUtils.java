// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.os.Build;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;

/** This class provides utilities for intenting into Google Lens. */
// TODO(crbug.com/40160855): Consolidate param-checks into a single function.
public class LensUtils {
    private static final String MIN_AGSA_VERSION_FEATURE_PARAM_NAME = "minAgsaVersionName";
    private static final String LOG_UKM_PARAM_NAME = "logUkm";
    private static final String ENABLE_ON_TABLET_PARAM_NAME = "enableContextMenuSearchOnTablet";
    private static final String DISABLE_ON_INCOGNITO_PARAM_NAME = "disableOnIncognito";
    private static final String ORDER_SHARE_IMAGE_BEFORE_LENS_PARAM_NAME =
            "orderShareImageBeforeLens";

    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE = "10.65";

    /** See function for details. */
    private static boolean sFakePassableLensEnvironmentForTesting;

    /*
     * If true, short-circuit the version name intent check to always return a high enough version.
     * Also hardcode the device OS check to return true.
     * Used by test cases.
     * @param shouldFake Whether to fake the version check.
     */
    public static void setFakePassableLensEnvironmentForTesting(final boolean shouldFake) {
        sFakePassableLensEnvironmentForTesting = shouldFake;
        ResettersForTesting.register(() -> sFakePassableLensEnvironmentForTesting = false);
    }

    /**
     * Resolve the activity to verify that lens is ready to accept an intent and
     * also retrieve the version name.
     *
     * @param context The relevant application context with access to the activity.
     * @return The version name string of the AGSA app or an empty string if not
     *         available.
     */
    public static String getLensActivityVersionNameIfAvailable(final Context context) {
        if (sFakePassableLensEnvironmentForTesting) {
            return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
        } else {
            if (context == null) {
                return "";
            }
            String agsaVersion = GSAUtils.getAgsaVersionName();
            if (agsaVersion == null) {
                return "";
            } else {
                return agsaVersion;
            }
        }
    }

    /**
     * Gets the minimum AGSA version required to support the Lens context menu
     * integration on this device. Takes the value from a server provided value if a
     * field trial is active but otherwise will take the value from a client side
     * default (unless the lens feature is not enabled at all, in which case return
     * an empty string).
     *
     * @return The minimum version name string or an empty string if not available.
     */
    public static String getMinimumAgsaVersionForLensSupport() {
        return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
    }

    /**
     * Checks whether the device is below Android O. We restrict to these versions
     * to limit to OS"s where image processing vulnerabilities can be retroactively
     * fixed if they are discovered in the future.
     *
     * @return Whether the device is below Android O.
     */
    public static boolean isDeviceOsBelowMinimum() {
        if (sFakePassableLensEnvironmentForTesting) {
            return false;
        }

        return Build.VERSION.SDK_INT < Build.VERSION_CODES.O;
    }

    /**
     * Checks whether the GSA package on the device is guaranteed to be an official
     * GSA build.
     *
     * @return Whether the package is valid.
     */
    public static boolean isValidAgsaPackage(final ExternalAuthUtils externalAuthUtils) {
        if (sFakePassableLensEnvironmentForTesting) {
            return true;
        }

        return externalAuthUtils.isGoogleSigned(IntentHandler.PACKAGE_GSA);
    }

    public static boolean isGoogleLensFeatureEnabled(boolean isIncognito) {
        return !isIncognito;
    }

    public static boolean shouldLogUkmForLensContextMenuFeatures() {
        return shouldLogUkmByFeature(ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS);
    }

    /*
     * Whether to log UKM pings for lens-related behavior.
     * If in the experiment will log by default and will only be disabled
     * if the parameter is not absent and set to true.
     * @param featureName The feature that uses the UKM reporting.
     */
    public static boolean shouldLogUkmByFeature(String featureName) {
        if (ChromeFeatureList.isEnabled(featureName)) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    featureName, LOG_UKM_PARAM_NAME, true);
        }
        return false;
    }
}
