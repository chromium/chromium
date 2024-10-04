// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabFeatureOverridesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.components.cached_flags.StringCachedFieldTrialParameter;

import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/** Utility methods for the Minimized Custom Tab feature. */
public class MinimizedFeatureUtils {
    public static final IntCachedFieldTrialParameter ICON_VARIANT =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.CCT_MINIMIZED, "icon_variant", 0);

    // Devices from this OEM--and potentially others--sometimes crash when we call
    // `Activity#enterPictureInPictureMode` on Android R. So, we disable the feature on those
    // devices. See: https://crbug.com/1519164.
    public static final StringCachedFieldTrialParameter MANUFACTURER_EXCLUDE_LIST =
            ChromeFeatureList.newStringCachedFieldTrialParameter(
                    ChromeFeatureList.CCT_MINIMIZED_ENABLED_BY_DEFAULT,
                    "manufacturer_exclude_list",
                    "xiaomi");

    private static Set<String> sManufacturerExcludeList;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        MinimizedFeatureAvailability.AVAILABLE,
        MinimizedFeatureAvailability.UNAVAILABLE_LOW_END_DEVICE,
        MinimizedFeatureAvailability.UNAVAILABLE_SYSTEM_FEATURE,
        MinimizedFeatureAvailability.UNAVAILABLE_PIP_PERMISSION,
        MinimizedFeatureAvailability.UNAVAILABLE_EXCLUDED_MANUFACTURER,
        MinimizedFeatureAvailability.NUM_ENTRIES
    })
    @VisibleForTesting
    @interface MinimizedFeatureAvailability {
        int AVAILABLE = 0;
        // (Obsolete) int UNAVAILABLE_API_LEVEL = 1;
        int UNAVAILABLE_LOW_END_DEVICE = 2;
        int UNAVAILABLE_SYSTEM_FEATURE = 3;
        int UNAVAILABLE_PIP_PERMISSION = 4;
        int UNAVAILABLE_EXCLUDED_MANUFACTURER = 5;

        int NUM_ENTRIES = 6;
    }

    private static Boolean sIsDeviceEligibleForMinimizedCustomTab;
    private static boolean sIsDeviceEligibleForMinimizedCustomTabForTesting;

    /**
     * Computes the availability of the Minimized Custom Tab feature based on multiple signals and
     * emits histograms accordingly.
     *
     * @param context The {@link Context}.
     * @param featureOverridesManager The {@link CustomTabFeatureOverridesManager} to check
     *     overridden features.
     * @return Whether the Minimized Custom Tab feature is available.
     */
    public static boolean isMinimizedCustomTabAvailable(
            Context context, @Nullable CustomTabFeatureOverridesManager featureOverridesManager) {
        if (!isDeviceEligibleForMinimizedCustomTab(context)) return false;
        if (!ChromeFeatureList.sCctIntentFeatureOverrides.isEnabled()) {
            return ChromeFeatureList.sCctMinimized.isEnabled();
        }
        if (featureOverridesManager == null) return ChromeFeatureList.sCctMinimized.isEnabled();

        Boolean override =
                featureOverridesManager.isFeatureEnabled(ChromeFeatureList.CCT_MINIMIZED);
        if (override != null) return override;
        return ChromeFeatureList.sCctMinimized.isEnabled();
    }

    /**
     * Computes the eligibility of the Minimized Custom Tab feature based on multiple signals and
     * emits histograms accordingly.
     *
     * @param context The {@link Context}.
     * @return Whether the device is eligible for Minimized Custom Tab feature.
     */
    private static boolean isDeviceEligibleForMinimizedCustomTab(Context context) {
        if (sIsDeviceEligibleForMinimizedCustomTabForTesting) return true;

        if (sIsDeviceEligibleForMinimizedCustomTab != null) {
            return sIsDeviceEligibleForMinimizedCustomTab;
        }

        @MinimizedFeatureAvailability int availability = MinimizedFeatureAvailability.AVAILABLE;
        sIsDeviceEligibleForMinimizedCustomTab = true;
        if (SysUtils.isLowEndDevice()) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_LOW_END_DEVICE;
            sIsDeviceEligibleForMinimizedCustomTab = false;
        } else if (!context.getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_SYSTEM_FEATURE;
            sIsDeviceEligibleForMinimizedCustomTab = false;
        } else if (!isPipAllowed(context)) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_PIP_PERMISSION;
            sIsDeviceEligibleForMinimizedCustomTab = false;
        } else if (isDeviceExcluded()) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_EXCLUDED_MANUFACTURER;
            sIsDeviceEligibleForMinimizedCustomTab = false;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.MinimizedFeatureAvailability",
                availability,
                MinimizedFeatureAvailability.NUM_ENTRIES);
        ResettersForTesting.register(() -> sIsDeviceEligibleForMinimizedCustomTab = null);
        return sIsDeviceEligibleForMinimizedCustomTab;
    }

    private static boolean isPipAllowed(Context context) {
        AppOpsManager appOpsManager =
                (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
        int result =
                appOpsManager.checkOpNoThrow(
                        AppOpsManager.OPSTR_PICTURE_IN_PICTURE,
                        context.getApplicationInfo().uid,
                        context.getPackageName());
        return result == AppOpsManager.MODE_ALLOWED;
    }

    private static boolean isDeviceExcluded() {
        sManufacturerExcludeList = new HashSet<>();
        String listStr = MANUFACTURER_EXCLUDE_LIST.getValue();
        if (!TextUtils.isEmpty(listStr)) {
            Collections.addAll(sManufacturerExcludeList, listStr.split(","));
        }
        return VERSION.SDK_INT == VERSION_CODES.R
                && (sManufacturerExcludeList.contains(Build.MANUFACTURER.toLowerCase(Locale.US))
                        || sManufacturerExcludeList.contains(Build.BRAND.toLowerCase(Locale.US)));
    }

    public static void setDeviceEligibleForMinimizedCustomTabForTesting(boolean eligibility) {
        sIsDeviceEligibleForMinimizedCustomTabForTesting = eligibility;
        ResettersForTesting.register(
                () -> sIsDeviceEligibleForMinimizedCustomTabForTesting = false);
    }

    public static @DrawableRes int getMinimizeIcon() {
        return ICON_VARIANT.getValue() == 1 ? R.drawable.ic_pip_24dp : R.drawable.ic_minimize;
    }

    /**
     * Returns whether Minimized Custom Tabs should be enabled based on the intent data provider.
     *
     * @param intentDataProvider The {@link BrowserServicesIntentDataProvider}.
     * @return Whether Minimized Custom Tabs should be enabled.
     */
    public static boolean shouldEnableMinimizedCustomTabs(
            BrowserServicesIntentDataProvider intentDataProvider) {
        boolean isWebApp =
                intentDataProvider.isWebappOrWebApkActivity()
                        || intentDataProvider.isTrustedWebActivity();
        if (isWebApp) return false;

        boolean isFedCmIntent =
                intentDataProvider.isTrustedIntent()
                        && IntentUtils.safeGetIntExtra(
                                        intentDataProvider.getIntent(),
                                        IntentHandler.EXTRA_FEDCM_ID,
                                        -1)
                                != -1;
        if (isFedCmIntent) return false;

        boolean isAuthTab = intentDataProvider.isAuthTab();
        if (isAuthTab) return false;

        return true;
    }
}
