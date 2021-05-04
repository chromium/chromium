// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.variations.VariationsAssociatedData;
import org.chromium.url.GURL;

/**
 * This class provides utilities for intenting into Google Lens.
 */
// TODO(crbug/1157496): Consolidate param-checks into a single function.
public class LensUtils {
    private static final String LENS_CONTRACT_URI = "googleapp://lens";
    private static final String LENS_DIRECT_INTENT_CONTRACT_URI = "google://lens";
    private static final String LENS_BITMAP_URI_KEY = "LensBitmapUriKey";
    private static final String ACCOUNT_NAME_URI_KEY = "AccountNameUriKey";
    private static final String INCOGNITO_URI_KEY = "IncognitoUriKey";
    private static final String LAUNCH_TIMESTAMP_URI_KEY = "ActivityLaunchTimestampNanos";
    private static final String IMAGE_SRC_URI_KEY = "ImageSrc";
    private static final String ALT_URI_KEY = "ImageAlt";
    private static final String PAGE_URI_KEY = "PageUrl";
    private static final String VARIATION_ID_URI_KEY = "Gid";
    private static final String LENS_INTENT_TYPE_KEY = "lens_intent_type";
    private static final String REQUIRE_ACCOUNT_DIALOG_KEY = "requiresConfirmation";

    private static final String MIN_AGSA_VERSION_FEATURE_PARAM_NAME = "minAgsaVersionName";
    private static final String MIN_AGSA_VERSION_SHOPPING_FEATURE_PARAM_NAME =
            "minAgsaVersionNameForShopping";
    private static final String MIN_AGSA_VERSION_DIRECT_INTENT_FEATURE_PARAM_NAME =
            "minAgsaVersionForDirectIntent";
    private static final String MIN_AGSA_VERSION_DIRECT_INTENT_SDK_FEATURE_PARAM_NAME =
            "minAgsaVersionForDirectIntentSdk";
    private static final String MIN_AGSA_VERSION_LENS_INTENT_API_FEATURE_PARAM_NAME =
            "minAgsaVersionForLensIntentApi";
    private static final String LENS_SHOPPING_ALLOWLIST_ENTRIES_FEATURE_PARAM_NAME =
            "allowlistEntries";
    private static final String LENS_SHOPPING_URL_PATTERNS_FEATURE_PARAM_NAME =
            "shoppingUrlPatterns";
    private static final String LOG_UKM_PARAM_NAME = "logUkm";
    private static final String SEND_SRC_PARAM_NAME = "sendSrc";
    private static final String SEND_ALT_PARAM_NAME = "sendAlt";
    private static final String SEND_PAGE_PARAM_NAME = "sendPage";
    private static final String USE_SEARCH_IMAGE_WITH_GOOGLE_LENS_ITEM_NAME_PARAM_NAME =
            "useSearchImageWithGoogleLensItemName";
    private static final String USE_DIRECT_INTENT_FEATURE_PARAM_NAME = "useDirectIntent";
    private static final String USE_DIRECT_INTENT_SDK_INTEGRATION_PARAM_NAME =
            "useDirectIntentSdkIntegration";
    private static final String DISABLE_ON_INCOGNITO_PARAM_NAME = "disableOnIncognito";
    private static final String ORDER_SHARE_IMAGE_BEFORE_LENS_PARAM_NAME =
            "orderShareImageBeforeLens";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE = "10.65";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_CHROME_SHOPPING_INTENT = "11.16";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_DIRECT_INTENT = "11.34";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_DIRECT_INTENT_SDK = "11.39.7";
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_INTENT_API = "12.10";
    private static final int LENS_INTENT_TYPE_LENS_CHROME_SHOPPING = 18;
    private static final String LENS_SHOPPING_FEATURE_FLAG_VARIANT_NAME = "lensShopVariation";
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
     * Gets the minimum AGSA version required to support the URI-based direct intentType
     * integration on this device. Takes the value from a server provided value if a
     * field trial is active but otherwise will take the value from a client side
     * default (unless the lens feature is not enabled at all, in which case return
     * an empty string).
     *
     * @return The minimum version name string or an empty string if not available.
     */
    public static String getMinimumAgsaVersionForDirectIntentSupport() {
        // Shopping feature AGSA version takes priority over Search with Google Lens
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)) {
            final String serverProvidedMinAgsaVersion =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                            MIN_AGSA_VERSION_DIRECT_INTENT_FEATURE_PARAM_NAME);
            if (TextUtils.isEmpty(serverProvidedMinAgsaVersion)) {
                // Falls into this block if the user enabled the feature using chrome://flags
                // and the param was not set by the server.
                return MIN_AGSA_VERSION_NAME_FOR_LENS_DIRECT_INTENT;
            }
            return serverProvidedMinAgsaVersion;
        }
        return "";
    }

    /**
     * Gets the minimum AGSA version required to support the direct intent SDK
     * integration on this device. Takes the value from a server provided value if a
     * field trial is active but otherwise will take the value from a client side
     * default (unless the lens feature is not enabled at all, in which case return
     * an empty string).
     *
     * @return The minimum version name string or an empty string if not available.
     */
    public static String getMinimumAgsaVersionForDirectIntentSdkSupport() {
        // Shopping feature AGSA version takes priority over Search with Google Lens
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)) {
            final String serverProvidedMinAgsaVersion =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                            MIN_AGSA_VERSION_DIRECT_INTENT_SDK_FEATURE_PARAM_NAME);
            if (TextUtils.isEmpty(serverProvidedMinAgsaVersion)) {
                // Falls into this block if the user enabled the feature using chrome://flags
                // and the param was not set by the server.
                return MIN_AGSA_VERSION_NAME_FOR_LENS_DIRECT_INTENT_SDK;
            }
            return serverProvidedMinAgsaVersion;
        }
        return "";
    }

    /**
     * Gets the minimum AGSA version required to support the LensIntent APIs
     * on this device. Takes the value from a server provided value if a
     * field trial is active but otherwise will take the value from a client side
     * default (unless the lens feature is not enabled at all, in which case return
     * an empty string).
     *
     * @return The minimum version name string or an empty string if not available.
     */
    public static String getMinimumAgsaVersionForLensIntentApiSupport() {
        final String serverProvidedMinAgsaVersion = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.GOOGLE_LENS_SDK_INTENT,
                MIN_AGSA_VERSION_LENS_INTENT_API_FEATURE_PARAM_NAME);
        if (TextUtils.isEmpty(serverProvidedMinAgsaVersion)) {
            // Falls into this block if the user enabled the feature using chrome://flags
            // and the param was not set by the server.
            return MIN_AGSA_VERSION_NAME_FOR_LENS_INTENT_API;
        }
        return serverProvidedMinAgsaVersion;
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
     * @param imageUri             The content provider URI generated by chrome (or
     *                             empty URI) if only resolving the activity.
     * @param isIncognito          Whether the current tab is in incognito mode.
     * @param currentTimeNanos     The current system time since boot in nanos.
     * @param srcUrl               The 'src' attribute of the image.
     * @param titleOrAltText       The 'title' or, if empty, the 'alt' attribute of the
     *                             image.
     * @param pageUrl              The url of the top level frame of the page.
     * @param lensEntryPoint       The entry point that launches the Lens app.
     * @param requiresConfirmation A boolean to indicate whether the request is from one of the
     *                             entry points that are not explicitly specified with
     *                             "Google Lens". We will show a confirmation dialog for this
     *                             request if true.
     * @return The intent to Google Lens.
     */
    public static Intent getShareWithGoogleLensIntent(final Context context, final Uri imageUri,
            final boolean isIncognito, final long currentTimeNanos, final GURL srcUrl,
            final String titleOrAltText, final GURL pageUrl, @LensEntryPoint int lensEntryPoint,
            boolean requiresConfirmation) {
        int lensIntentType = lensEntryPoint == LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM
                ? LensUtils.getLensShoppingIntentType()
                : 0;

        final CoreAccountInfo coreAccountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC);
        // If incognito do not send the account name to avoid leaking session
        // information to Lens.
        final String signedInAccountName =
                (coreAccountInfo == null || isIncognito) ? "" : coreAccountInfo.getEmail();
        Uri lensUri = useDirectIntent(context) ? Uri.parse(LENS_DIRECT_INTENT_CONTRACT_URI)
                                               : Uri.parse(LENS_CONTRACT_URI);
        if (!Uri.EMPTY.equals(imageUri)) {
            final Uri.Builder lensUriBuilder =
                    lensUri.buildUpon()
                            .appendQueryParameter(LENS_BITMAP_URI_KEY, imageUri.toString())
                            .appendQueryParameter(ACCOUNT_NAME_URI_KEY, signedInAccountName)
                            .appendQueryParameter(INCOGNITO_URI_KEY, Boolean.toString(isIncognito))
                            .appendQueryParameter(
                                    LAUNCH_TIMESTAMP_URI_KEY, Long.toString(currentTimeNanos));

            if (lensIntentType > 0) {
                lensUriBuilder.appendQueryParameter(
                        LENS_INTENT_TYPE_KEY, Integer.toString(lensIntentType));
            }

            if (requiresConfirmation) {
                lensUriBuilder.appendQueryParameter(
                        REQUIRE_ACCOUNT_DIALOG_KEY, Boolean.toString(requiresConfirmation));
            }

            if (!isIncognito) {
                if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                            SEND_SRC_PARAM_NAME, false)) {
                    lensUriBuilder.appendQueryParameter(IMAGE_SRC_URI_KEY, srcUrl.getSpec());
                }
                if ((titleOrAltText != null)
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                                SEND_ALT_PARAM_NAME, false)) {
                    lensUriBuilder.appendQueryParameter(ALT_URI_KEY, titleOrAltText);
                }
                if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                            SEND_PAGE_PARAM_NAME, false)) {
                    lensUriBuilder.appendQueryParameter(PAGE_URI_KEY, pageUrl.getSpec());
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

    /**
     * Start an early Lens AGSA connection if feature parameter is enabled and client is not
     * incognito. Eligibity checks happen in LensController.
     *
     * @param isIncognito Whether the client is incognito
     */
    public static void startLensConnectionIfNecessary(boolean isIncognito) {
        // TODO(crbug/1157543): Pass isIncognito through to LensController.
        if (!isIncognito) {
            AppHooks.get().getLensController().startLensConnection();
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
            AppHooks.get().getLensController().terminateLensConnections();
        }
    }

    /**
     * Build a LensIntentParams object from the provided parameters in order to intent into Lens.
     *
     * @param imageUri         The content provider URI generated by chrome (or
     *                         empty URI) if only resolving the activity.
     * @param isIncognito      Whether the current tab is in incognito mode.
     * @param srcUrl           The 'src' attribute of the image.
     * @param titleOrAltText   The 'title' or, if empty, the 'alt' attribute of the
     *                         image.
     * @param pageUrl          The url of the top level frame of the page.
     * @param lensEntryPoint   The entry point that launches the Lens app.
     * @param requiresConfirmation A boolean to indicate whether the request is from one of the
     *                             entry points that are not explicitly specified with
     *                             "Google Lens". We will show a confirmation dialog for this
     *                             request if true.
     * @return The intent parameters to intent to Google Lens.
     */
    public static LensIntentParams buildLensIntentParams(final Uri imageUri,
            final boolean isIncognito, final String srcUrl, final String titleOrAltText,
            final String pageUrl, @LensEntryPoint int lensEntryPoint,
            boolean requiresConfirmation) {
        // TODO(yusuyoutube): deprecate lensIntentType once we have the mapping for LensEntryPoint
        // to intent type in the Lens closed source repository.
        int lensIntentType = lensEntryPoint == LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM
                ? LensUtils.getLensShoppingIntentType()
                : 0;

        LensIntentParams.Builder intentParamsBuilder =
                new LensIntentParams.Builder(lensEntryPoint, isIncognito);
        return intentParamsBuilder.withImageUri(imageUri)
                .withRequiresConfirmation(requiresConfirmation)
                .withIntentType(lensIntentType)
                .withImageTitleOrAltText(titleOrAltText)
                .withSrcUrl(srcUrl)
                .withPageUrl(pageUrl)
                .build();
    }

    public static boolean isGoogleLensFeatureEnabled(boolean isIncognito) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)
                && !(isIncognito
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                                DISABLE_ON_INCOGNITO_PARAM_NAME, true));
    }

    public static boolean isGoogleLensShoppingFeatureEnabled(boolean isIncognito) {
        return (useLensWithShopSimilarProducts() || useLensWithShopImageWithGoogleLens()
                       || useLensWithSearchSimilarProducts())
                && !(isIncognito
                        && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS,
                                DISABLE_ON_INCOGNITO_PARAM_NAME, true))
                // Dont enable both the chip and the shopping menu item.
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP);
    }

    /**
     * Enables the starting of LenActivity directly, rather than going through the Lens
     * session running in AGSA. Also checks if the required AGSA version for direct intent
     * is below or equal to the provided version.
     */
    public static boolean useDirectIntent(final Context context) {
        // TODO(https://crbug.com/1146591): Refactor GSA state checks to avoid multiple version
        // grabs.
        String agsaVersionName = sFakeInstalledAgsaVersion != null
                ? sFakeInstalledAgsaVersion
                : getLensActivityVersionNameIfAvailable(context);
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                       ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                       USE_DIRECT_INTENT_FEATURE_PARAM_NAME, false)
                && !GSAState.getInstance(context).isAgsaVersionBelowMinimum(
                        agsaVersionName, getMinimumAgsaVersionForDirectIntentSupport());
    }

    /**
     * Enables the starting of LenActivity directly, rather than going through the Lens
     * session running in AGSA. Also checks if the required AGSA version for direct intent
     * is below or equal to the provided version. This feature will not be launched and is
     * experimental.
     */
    @Deprecated
    public static boolean useDirectIntentSdkIntegration(final Context context) {
        // TODO(https://crbug.com/1146591): Refactor GSA state checks to avoid multiple version
        // grabs.
        String agsaVersionName = sFakeInstalledAgsaVersion != null
                ? sFakeInstalledAgsaVersion
                : getLensActivityVersionNameIfAvailable(context);
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                       ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                       USE_DIRECT_INTENT_SDK_INTEGRATION_PARAM_NAME, false)
                && !GSAState.getInstance(context).isAgsaVersionBelowMinimum(
                        agsaVersionName, getMinimumAgsaVersionForDirectIntentSdkSupport());
    }

    /**
     * Enables the starting of LenActivity via LensIntent API, rather than using a deeplink or
     * bundle clients via SDK. This will allow Chrome to share the intent library with other
     * surfaces, while still entering LensActivity directly.
     */
    public static boolean useLensIntentApi() {
        // TODO(https://crbug.com/1146591): Refactor GSA state checks to avoid multiple version
        // grabs.
        String agsaVersionName = sFakeInstalledAgsaVersion != null
                ? sFakeInstalledAgsaVersion
                : getLensActivityVersionNameIfAvailable(ContextUtils.getApplicationContext());
        return ChromeFeatureList.isEnabled(ChromeFeatureList.GOOGLE_LENS_SDK_INTENT)
                && !GSAState.getInstance(ContextUtils.getApplicationContext())
                            .isAgsaVersionBelowMinimum(agsaVersionName,
                                    getMinimumAgsaVersionForLensIntentApiSupport());
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
     * Whether to display the lens shop image with google lens chip.
     */
    public static boolean enableImageChip() {
        // TODO(benwgold): Consider adding isSdkAvailable() check if it gains any utility.
        //                 Currently it is not necessary and should always evaluate to true.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP);
    }

    /**
     * Whether to display the Lens translate with Google Lens chip.
     */
    public static boolean enableTranslateChip() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS);
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

    /*
     * Whether to log UKM pings for lens-related behavior.
     * If in the experiment will log by default and will only be disabled
     * if the parameter is not absent and set to true.
     * @param isIncognito Whether the user is currently in incognito mode.
     */
    public static boolean shouldLogUkm(boolean isIncognito) {
        // Lens shopping feature takes the priority over the "Search image with Google Lens".
        if (enableImageChip()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP, LOG_UKM_PARAM_NAME, true);
        }

        if (enableTranslateChip()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS, LOG_UKM_PARAM_NAME,
                    true);
        }

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
