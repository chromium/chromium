// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.url.GURL;

/**
 * This class provides utilities for intenting into Google Lens.
 */
// TODO(crbug/1157496): Consolidate param-checks into a single function.
public class LensUtils {
    private static final String MIN_AGSA_VERSION_FEATURE_PARAM_NAME = "minAgsaVersionName";
    private static final String MIN_AGSA_VERSION_SHOPPING_FEATURE_PARAM_NAME =
            "minAgsaVersionNameForShopping";
    private static final String LENS_SHOPPING_ALLOWLIST_ENTRIES_FEATURE_PARAM_NAME =
            "allowlistEntries";
    private static final String LENS_SHOPPING_URL_PATTERNS_FEATURE_PARAM_NAME =
            "shoppingUrlPatterns";
    private static final String LOG_UKM_PARAM_NAME = "logUkm";
    private static final String USE_SEARCH_IMAGE_WITH_GOOGLE_LENS_ITEM_NAME_PARAM_NAME =
            "useSearchImageWithGoogleLensItemName";
    private static final String ENABLE_ON_TABLET_PARAM_NAME = "enableContextMenuSearchOnTablet";
    private static final String DISABLE_ON_INCOGNITO_PARAM_NAME = "disableOnIncognito";
    private static final String ORDER_SHARE_IMAGE_BEFORE_LENS_PARAM_NAME =
            "orderShareImageBeforeLens";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE = "10.65";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_CHROME_SHOPPING_INTENT = "11.16";
    private static final int LENS_INTENT_TYPE_LENS_CHROME_SHOPPING = 18;
    private static final String LENS_DEFAULT_SHOPPING_URL_PATTERNS =
            "^https://www.google.com/shopping/.*|^https://www.google.com/.*tbm=shop.*";

    /**
     * See function for details.
     */
    private static boolean sFakePassableLensEnvironmentForTesting;
    private static boolean sFakeImageUrlInShoppingAllowlistForTesting;
    private static String sFakeInstalledAgsaVersion;
    private static String sFakeVariationsForTesting;

    /*
     * If true, short-circuit the version name intent check to always return a high enough version.
     * Also hardcode the device OS check to return true.
     * Used by test cases.
     * @param shouldFake Whether to fake the version check.
     */
    public static void setFakePassableLensEnvironmentForTesting(final boolean shouldFake) {
        sFakePassableLensEnvironmentForTesting = shouldFake;
    }

    @VisibleForTesting
    public static void setFakeImageUrlInShoppingAllowlistForTesting(final boolean shouldFake) {
        sFakeImageUrlInShoppingAllowlistForTesting = shouldFake;
    }

    /**
     * Sets a fake installed agsa version name. Used by test cases to set versions below and above
     * minimum required agsa versions.
     */
    @VisibleForTesting
    public static void setFakeInstalledAgsaVersion(final String fakeAgsaVersionName) {
        sFakeInstalledAgsaVersion = fakeAgsaVersionName;
    }

    /*
     * If set, short-circuit the JNI call to retrieve the variation IDs. Used by
     * test cases.
     *
     * @param fakeIdString Whether to fake the version check.
     */
    @VisibleForTesting
    static void setFakeVariationsForTesting(final String fakeVariations) {
        sFakeVariationsForTesting = fakeVariations;
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
        if (Boolean.TRUE.equals(sFakePassableLensEnvironmentForTesting)) {
            // Returns the minimum version which will meet the bar and allow future AGSA
            // version
            // checks to succeed.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS)) {
                return MIN_AGSA_VERSION_NAME_FOR_LENS_CHROME_SHOPPING_INTENT;
            }
            return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
        } else {
            if (context == null) {
                return "";
            }
            String agsaVersion = GSAState.getInstance(context).getAgsaVersionName();
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)) {
            final String serverProvidedMinAgsaVersion =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                            MIN_AGSA_VERSION_FEATURE_PARAM_NAME);
            if (TextUtils.isEmpty(serverProvidedMinAgsaVersion)) {
                // Falls into this block if the user enabled the feature using chrome://flags
                // and the param was not set by the server.
                return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
            }
            return serverProvidedMinAgsaVersion;
        }
        // The feature is disabled so no need to return a minimum version.
        return "";
    }

    /**
     * Gets the minimum AGSA version required to support the Lens shopping context menu
     * integration on this device. Takes the value from a server provided value if a
     * field trial is active but otherwise will take the value from a client side
     * default (unless the lens feature is not enabled at all, in which case return
     * an empty string).
     *
     * @return The minimum version name string or an empty string if not available.
     */
    public static String getMinimumAgsaVersionForLensShoppingSupport() {
        // Shopping feature AGSA version takes priority over Search with Google Lens
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS)) {
            final String serverProvidedMinAgsaVersion =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS,
                            MIN_AGSA_VERSION_SHOPPING_FEATURE_PARAM_NAME);
            if (TextUtils.isEmpty(serverProvidedMinAgsaVersion)) {
                // Falls into this block if the user enabled the feature using chrome://flags
                // and the param was not set by the server.
                return MIN_AGSA_VERSION_NAME_FOR_LENS_CHROME_SHOPPING_INTENT;
            }
            return serverProvidedMinAgsaVersion;
        }
        return "";
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

    /**
     * Start an early Lens AGSA connection if feature parameter is enabled and client is not
     * incognito. Eligibity checks happen in LensController.
     *
     * @param isIncognito Whether the client is incognito
     */
    public static void startLensConnectionIfNecessary(boolean isIncognito) {
        // TODO(crbug/1157543): Pass isIncognito through to LensController.
        if (!isIncognito) {
            LensController.getInstance().startLensConnection();
        }
    }

    /**
     * Terminate an early Lens AGSA connection if feature parameter is enabled and client is not
     * incognito. Eligibity checks happen in LensController.
     *
     * @param isIncognito Whether the client is incognito
     */
    public static void terminateLensConnectionsIfNecessary(boolean isIncognito) {
        // TODO(crbug/1157543): Pass isIncognito through to LensController.
        if (!isIncognito) {
            LensController.getInstance().terminateLensConnections();
        }
    }

    public static boolean isGoogleLensFeatureEnabled(boolean isIncognito) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)
                && !(isIncognito
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                                DISABLE_ON_INCOGNITO_PARAM_NAME, true));
    }

    public static boolean isGoogleLensFeatureEnabledOnTablet() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS, ENABLE_ON_TABLET_PARAM_NAME,
                false);
    }

    public static boolean isGoogleLensShoppingFeatureEnabled(boolean isIncognito) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS)) {
            return false;
        }

        // Dont enable both the chip and the shopping menu item.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP)) {
            return false;
        }

        // Disable on Incognito.
        if (isIncognito
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS,
                        DISABLE_ON_INCOGNITO_PARAM_NAME, true)) {
            return false;
        }

        return true;
    }

    /**
     * Adjust chip ordering slightly. The image chip feature changes the context menu height
     * which can result  in the final image menu items being hidden in certain contexts.
     * @return Whether to list 'Share Image' above 'Search with Google Lens'.
     */
    public static boolean orderShareImageBeforeLens() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP,
                ORDER_SHARE_IMAGE_BEFORE_LENS_PARAM_NAME, false);
    }

    /**
     * Gets the list of Allowlisted Entries as String. Format is a comma separated
     * list of Entry names (as strings).
     */
    public static String getAllowlistEntries() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST,
                LENS_SHOPPING_ALLOWLIST_ENTRIES_FEATURE_PARAM_NAME);
    }

    /**
     * Gets the list of shopping url patterns(regex) as String. Format is a "||" separated
     * list of regex strings.
     */
    public static String getShoppingUrlPatterns() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST,
                LENS_SHOPPING_URL_PATTERNS_FEATURE_PARAM_NAME);
    }

    /**
     * Check if the Lens shopping allowlist feature is enabled.
     * The shopping allowlist is used to determine whether the image is shoppable.
     * @return true if the shopping allowlist feature is enabled.
     */
    public static boolean isShoppingAllowlistEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST);
    }

    /**
     * Check if the page uri to determine whether the image is shoppable.
     * @return true if the image is shoppable.
     */
    public static boolean isInShoppingAllowlist(final GURL url) {
        if (sFakeImageUrlInShoppingAllowlistForTesting) {
            return true;
        }

        if (!isShoppingAllowlistEnabled()) {
            return false;
        }

        if (GURL.isEmptyOrInvalid(url)) {
            return false;
        }

        return hasShoppingUrlPattern(url) || isInDomainAllowList(url);
    }

    public static boolean shouldLogUkmForLensContextMenuFeatures() {
        return shouldLogUkmByFeature(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)
                || shouldLogUkmByFeature(ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS)
                || shouldLogUkmByFeature(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP)
                || shouldLogUkmByFeature(ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS);
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

    /**
     * @return the Lens shopping intent type integer.
     */
    public static int getLensShoppingIntentType() {
        return LENS_INTENT_TYPE_LENS_CHROME_SHOPPING;
    }

    /**
     * Check if the the intent type is Lens shopping intent type.
     * @return true if the intent type is shopping.
     */
    public static boolean isLensShoppingIntentType(int intentType) {
        return intentType == getLensShoppingIntentType();
    }

    /**
     * Check if the uri matches any shopping url patterns.
     */
    private static boolean hasShoppingUrlPattern(final GURL url) {
        String shoppingUrlPatterns = getShoppingUrlPatterns();
        if (shoppingUrlPatterns == null || shoppingUrlPatterns.isEmpty()) {
            // Fallback to default shopping url patterns.
            shoppingUrlPatterns = LENS_DEFAULT_SHOPPING_URL_PATTERNS;
        }

        return url.getSpec().matches(shoppingUrlPatterns);
    }

    /**
     * Check if the uri domain is in the Lens shopping domain Allowlist.
     */
    private static boolean isInDomainAllowList(final GURL url) {
        final String allowlistEntries = getAllowlistEntries();
        final String[] allowlist = allowlistEntries.split(",");

        for (final String allowlistEntry : allowlist) {
            if (allowlistEntry.length() > 0 && url.getSpec().contains(allowlistEntry)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Check if experiment to change Lens context menu item name is enabled.
     */
    public static boolean useSearchImageWithGoogleLensItemName() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                USE_SEARCH_IMAGE_WITH_GOOGLE_LENS_ITEM_NAME_PARAM_NAME, false);
    }
}
