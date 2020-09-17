// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.variations.VariationsAssociatedData;

/**
 * This class provides utilities for intenting into Google Lens.
 */
public class LensUtils {
    private static final String LENS_CONTRACT_URI = "googleapp://lens";
    private static final String LENS_DIRECT_INTENT_CONTRACT_URI = "google://lens";
    private static final String LENS_BITMAP_URI_KEY = "LensBitmapUriKey";
    private static final String ACCOUNT_NAME_URI_KEY = "AccountNameUriKey";
    private static final String INCOGNITO_URI_KEY = "IncognitoUriKey";
    private static final String LAUNCH_TIMESTAMP_URI_KEY = "ActivityLaunchTimestampNanos";
    private static final String IMAGE_SRC_URI_KEY = "ImageSrc";
    private static final String ALT_URI_KEY = "ImageAlt";
    private static final String VARIATION_ID_URI_KEY = "Gid";
    private static final String LENS_INTENT_TYPE_KEY = "lens_intent_type";
    private static final String REQUIRE_ACCOUNT_DIALOG_KEY = "requiresConfirmation";

    private static final String MIN_AGSA_VERSION_FEATURE_PARAM_NAME = "minAgsaVersionName";
    private static final String MIN_AGSA_VERSION_SHOPPING_FEATURE_PARAM_NAME =
            "minAgsaVersionNameForShopping";
    private static final String USE_SEARCH_BY_IMAGE_TEXT_FEATURE_PARAM_NAME =
            "useSearchByImageText";
    private static final String LENS_SHOPPING_ALLOWLIST_ENTRIES_FEATURE_PARAM_NAME =
            "allowlistEntries";
    private static final String LENS_SHOPPING_URL_PATTERNS_FEATURE_PARAM_NAME =
            "shoppingUrlPatterns";
    private static final String LOG_UKM_PARAM_NAME = "logUkm";
    private static final String SEND_SRC_PARAM_NAME = "sendSrc";
    private static final String SEND_ALT_PARAM_NAME = "sendAlt";
    private static final String USE_DIRECT_INTENT_FEATURE_PARAM_NAME = "useDirectIntent";
    private static final String DISABLE_ON_INCOGNITO_PARAM_NAME = "disableOnIncognito";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE = "8.19";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_CHROME_SHOPPING_INTENT = "11.16";
    private static final String LENS_INTENT_TYPE_LENS_CHROME_SHOPPING = "18";
    private static final String LENS_SHOPPING_FEATURE_FLAG_VARIANT_NAME = "lensShopVariation";
    private static final String LENS_DEFAULT_SHOPPING_URL_PATTERNS =
            "^https://www.google.com/shopping/.*|^https://www.google.com/.*tbm=shop.*";

    /**
     * See function for details.
     */
    private static boolean sFakePassableLensEnvironmentForTesting;
    private static boolean sFakeImageUrlInShoppingAllowlistForTesting;
    private static String sFakeVariationsForTesting;
    /** Supported Lens intent types. */
    @IntDef({
            IntentType.DEFAULT,
            IntentType.SHOPPING,
    })
    public @interface IntentType {
        int DEFAULT = 0;
        int SHOPPING = 1;
    }

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
        // Use this syntax to avoid NPE if unset.
        if (Boolean.TRUE.equals(sFakePassableLensEnvironmentForTesting)) {
            // Returns the minimum version which will meet the bar and allow future AGSA
            // version
            // checks to succeed.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS)) {
                return MIN_AGSA_VERSION_NAME_FOR_LENS_CHROME_SHOPPING_INTENT;
            }
            return MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE;
        } else {
            try {
                final PackageManager pm = context.getPackageManager();
                // No data transmission occurring so safe to assume incognito is false.
                final Intent lensIntent =
                        getShareWithGoogleLensIntent(Uri.EMPTY, /* isIncognito= */ false,
                                /* currentTimeNanos= */ 0L, /* srcUrl */ "",
                                /* titleOrAltText */ "", /* intentType */ IntentType.DEFAULT,
                                /* requiresConfirmation */ false);
                final ComponentName lensActivity = lensIntent.resolveActivity(pm);
                if (lensActivity == null) return "";
                final PackageInfo packageInfo = pm.getPackageInfo(lensActivity.getPackageName(), 0);
                if (packageInfo == null) {
                    return "";
                } else {
                    return packageInfo.versionName;
                }
            } catch (final PackageManager.NameNotFoundException e) {
                return "";
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
     * Get a deeplink intent to Google Lens with an optional content provider image
     * URI. The intent should be constructed immediately before the intent is fired
     * to ensure that the launch timestamp is accurate.
     *
     * @param imageUri         The content provider URI generated by chrome (or
     *                         empty URI) if only resolving the activity.
     * @param isIncognito      Whether the current tab is in incognito mode.
     * @param currentTimeNanos The current system time since boot in nanos.
     * @param srcUrl           The 'src' attribute of the image.
     * @param titleOrAltText   The 'title' or, if empty, the 'alt' attribute of the
     *                         image.
     * @param intentType The type of the intent.
     * @param requiresConfirmation Whether the request requires an confirmation dialog.
     * @return The intent to Google Lens.
     */

    public static Intent getShareWithGoogleLensIntent(final Uri imageUri, final boolean isIncognito,
            final long currentTimeNanos, final String srcUrl, final String titleOrAltText,
            @IntentType final int intentType, final boolean requiresConfirmation) {
        final CoreAccountInfo coreAccountInfo =
                IdentityServicesProvider.get().getIdentityManager().getPrimaryAccountInfo(
                        ConsentLevel.SYNC);
        // If incognito do not send the account name to avoid leaking session
        // information to Lens.
        final String signedInAccountName =
                (coreAccountInfo == null || isIncognito) ? "" : coreAccountInfo.getEmail();

        Uri lensUri = useDirectIntent() ? Uri.parse(LENS_DIRECT_INTENT_CONTRACT_URI)
                                        : Uri.parse(LENS_CONTRACT_URI);
        if (!Uri.EMPTY.equals(imageUri)) {
            final Uri.Builder lensUriBuilder =
                    lensUri.buildUpon()
                            .appendQueryParameter(LENS_BITMAP_URI_KEY, imageUri.toString())
                            .appendQueryParameter(ACCOUNT_NAME_URI_KEY, signedInAccountName)
                            .appendQueryParameter(INCOGNITO_URI_KEY, Boolean.toString(isIncognito))
                            .appendQueryParameter(
                                    LAUNCH_TIMESTAMP_URI_KEY, Long.toString(currentTimeNanos));

            if (intentType == IntentType.SHOPPING) {
                lensUriBuilder.appendQueryParameter(
                        LENS_INTENT_TYPE_KEY, LENS_INTENT_TYPE_LENS_CHROME_SHOPPING);
            }

            if (requiresConfirmation) {
                lensUriBuilder.appendQueryParameter(
                        REQUIRE_ACCOUNT_DIALOG_KEY, Boolean.toString(requiresConfirmation));
            }

            if (!isIncognito) {
                if ((srcUrl != null)
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                                SEND_SRC_PARAM_NAME, false)) {
                    lensUriBuilder.appendQueryParameter(IMAGE_SRC_URI_KEY, srcUrl);
                }
                if ((titleOrAltText != null)
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                                SEND_ALT_PARAM_NAME, false)) {
                    lensUriBuilder.appendQueryParameter(ALT_URI_KEY, titleOrAltText);
                }
                String variations = sFakeVariationsForTesting == null
                        ? VariationsAssociatedData.getGoogleAppVariations()
                        : sFakeVariationsForTesting;
                variations = variations.trim();
                if (!variations.isEmpty()) {
                    lensUriBuilder.appendQueryParameter(VARIATION_ID_URI_KEY, variations);
                }
                lensUri = lensUriBuilder.build();
            }

            lensUri = lensUriBuilder.build();
            ContextUtils.getApplicationContext().grantUriPermission(
                    IntentHandler.PACKAGE_GSA, imageUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        ContextUtils.getApplicationContext().grantUriPermission(
                IntentHandler.PACKAGE_GSA, imageUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        final Intent intent = new Intent(Intent.ACTION_VIEW).setData(lensUri);
        intent.setPackage(IntentHandler.PACKAGE_GSA);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        return intent;
    }

    public static boolean isGoogleLensFeatureEnabled(boolean isIncognito) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)
                && !(isIncognito
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                                DISABLE_ON_INCOGNITO_PARAM_NAME, false));
    }

    public static boolean isGoogleLensShoppingFeatureEnabled(boolean isIncognito) {
        return (useLensWithShopSimilarProducts() || useLensWithShopImageWithGoogleLens()
                       || useLensWithSearchSimilarProducts())
                && !(isIncognito
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS,
                                DISABLE_ON_INCOGNITO_PARAM_NAME, false));
    }

    /**
     * Enables the starting of LenActivity directly, rather than going through the Lens
     * session running in AGSA.
     */
    public static boolean useDirectIntent() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                USE_DIRECT_INTENT_FEATURE_PARAM_NAME, false);
    }

    /**
     * Whether to display the lens menu item with the search by image text
     */
    public static boolean useLensWithSearchByImageText() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                USE_SEARCH_BY_IMAGE_TEXT_FEATURE_PARAM_NAME, false);
    }

    /**
     * Whether to display the lens menu item shop similar products. only one of the
     * 3 params should be set to true: useLensWithShopSimilarProducts,
     * useLensWithShopImageWithGoogleLens and useLensWithShopImageWithGoogleLens.
     */
    public static boolean useLensWithShopSimilarProducts() {
        String variation = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS,
                LENS_SHOPPING_FEATURE_FLAG_VARIANT_NAME);
        return variation.equals("ShopSimilarProducts");
    }

    /**
     * Whether to display the lens menu item shop image with google lens.
     */
    public static boolean useLensWithShopImageWithGoogleLens() {
        String variation = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS,
                LENS_SHOPPING_FEATURE_FLAG_VARIANT_NAME);
        return variation.equals("ShopImageWithGoogleLens");
    }

    /**
     * Whether to display the lens menu item search similar products.
     */
    public static boolean useLensWithSearchSimilarProducts() {
        String variation = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS,
                LENS_SHOPPING_FEATURE_FLAG_VARIANT_NAME);
        return variation.equals("SearchSimilarProducts");
    }

    public static boolean showBothSearchAndShopImageWithLens() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_AND_SHOP_WITH_GOOGLE_LENS);
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
    public static boolean isInShoppingAllowlist(final String url) {
        if (sFakeImageUrlInShoppingAllowlistForTesting) {
            return true;
        }

        if (!isShoppingAllowlistEnabled()) {
            return false;
        }

        if (url == null || url.isEmpty()) {
            return false;
        }

        return hasShoppingUrlPattern(url) || isInDomainAllowList(url);
    }

    /*
     * Whether to log UKM pings for lens-related behavior.
     * If in the experiment will log by default and will only be disabled
     * if the parameter is not absent and set to true.
     * @param isIncognito Whether the user is currently in incognito mode.
     */
    public static boolean shouldLogUkm(boolean isIncognito) {
        // Lens shopping feature takes the priority over the "Search image with Google Lens".
        if (isGoogleLensShoppingFeatureEnabled(isIncognito)) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS, LOG_UKM_PARAM_NAME, true);
        }
        if (isGoogleLensFeatureEnabled(isIncognito)) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS, LOG_UKM_PARAM_NAME,
                    true);
        }

        return false;
    }

    /**
     * Check if the uri matches any shopping url patterns.
     */
    private static boolean hasShoppingUrlPattern(final String url) {
        String shoppingUrlPatterns = getShoppingUrlPatterns();
        if (shoppingUrlPatterns == null || shoppingUrlPatterns.isEmpty()) {
            // Fallback to default shopping url patterns.
            shoppingUrlPatterns = LENS_DEFAULT_SHOPPING_URL_PATTERNS;
        }

        return url.matches(shoppingUrlPatterns);
    }

    /**
     * Check if the uri domain is in the Lens shopping domain Allowlist.
     */
    private static boolean isInDomainAllowList(final String url) {
        final String allowlistEntries = getAllowlistEntries();
        final String[] allowlist = allowlistEntries.split(",");

        for (final String allowlistEntry : allowlist) {
            if (allowlistEntry.length() > 0 && url.contains(allowlistEntry)) {
                return true;
            }
        }

        return false;
    }
}
