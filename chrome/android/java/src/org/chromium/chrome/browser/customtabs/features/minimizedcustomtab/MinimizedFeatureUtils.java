// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;

/** Utility methods for the Minimized Custom Tab feature. */
public class MinimizedFeatureUtils {
    public static final IntCachedFieldTrialParameter ICON_VARIANT =
            new IntCachedFieldTrialParameter(ChromeFeatureList.CCT_MINIMIZED, "icon_variant", 0);

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        MinimizedFeatureAvailability.AVAILABLE,
        MinimizedFeatureAvailability.UNAVAILABLE_API_LEVEL,
        MinimizedFeatureAvailability.UNAVAILABLE_LOW_END_DEVICE,
        MinimizedFeatureAvailability.UNAVAILABLE_SYSTEM_FEATURE,
        MinimizedFeatureAvailability.UNAVAILABLE_PIP_PERMISSION,
        MinimizedFeatureAvailability.NUM_ENTRIES
    })
    @VisibleForTesting
    @interface MinimizedFeatureAvailability {
        int AVAILABLE = 0;
        int UNAVAILABLE_API_LEVEL = 1;
        int UNAVAILABLE_LOW_END_DEVICE = 2;
        int UNAVAILABLE_SYSTEM_FEATURE = 3;
        int UNAVAILABLE_PIP_PERMISSION = 4;

        int NUM_ENTRIES = 5;
    }

    private static Boolean sIsMinimizedCustomTabAvailable;
    private static boolean sMinimizedCustomTabAvailableForTesting;

    /**
     * Computes the availability of the Minimized Custom Tab feature based on multiple signals and
     * emits histograms accordingly.
     *
     * @param context The {@link Context}.
     * @return Whether the Minimized Custom Tab feature is available.
     */
    public static boolean isMinimizedCustomTabAvailable(Context context) {
        if (sMinimizedCustomTabAvailableForTesting) return true;

        if (sIsMinimizedCustomTabAvailable != null) {
            return sIsMinimizedCustomTabAvailable;
        }

        @MinimizedFeatureAvailability int availability;

        if (VERSION.SDK_INT < VERSION_CODES.O) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_API_LEVEL;
            sIsMinimizedCustomTabAvailable = false;
        } else if (SysUtils.isLowEndDevice()) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_LOW_END_DEVICE;
            sIsMinimizedCustomTabAvailable = false;
        } else if (!context.getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_SYSTEM_FEATURE;
            sIsMinimizedCustomTabAvailable = false;
        } else if (!isPipAllowed(context)) {
            availability = MinimizedFeatureAvailability.UNAVAILABLE_PIP_PERMISSION;
            sIsMinimizedCustomTabAvailable = false;
        } else {
            availability = MinimizedFeatureAvailability.AVAILABLE;
            sIsMinimizedCustomTabAvailable = ChromeFeatureList.sCctMinimized.isEnabled();
        }

        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.MinimizedFeatureAvailability",
                availability,
                MinimizedFeatureAvailability.NUM_ENTRIES);
        ResettersForTesting.register(() -> sIsMinimizedCustomTabAvailable = null);
        return sIsMinimizedCustomTabAvailable;
    }

    @RequiresApi(api = VERSION_CODES.O)
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

    public static void setMinimizeCustomTabAvailableForTesting(boolean availability) {
        sMinimizedCustomTabAvailableForTesting = availability;
        ResettersForTesting.register(() -> sMinimizedCustomTabAvailableForTesting = false);
    }

    public static @DrawableRes int getMinimizeIcon() {
        return ICON_VARIANT.getValue() == 1 ? R.drawable.ic_pip_24dp : R.drawable.ic_minimize;
    }
}
