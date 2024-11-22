// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;

import java.io.ByteArrayOutputStream;
import java.io.File;

public class AuxiliarySearchUtils {
    @VisibleForTesting static final String TAB_DONATE_FILE_NAME = "tabs_donate";
    @VisibleForTesting static final int DEFAULT_TTL_HOURS = 168;

    private static final String CONTENT_TTL_HOURS_PARAM = "content_ttl_hours";
    public static final IntCachedFieldTrialParameter CONTENT_TTL_HOURS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_V2,
                    CONTENT_TTL_HOURS_PARAM,
                    DEFAULT_TTL_HOURS);

    private static final String ZERO_STATE_FAVICON_NUMBER_PARAM = "zero_state_favicon_number";
    public static final IntCachedFieldTrialParameter ZERO_STATE_FAVICON_NUMBER =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    ZERO_STATE_FAVICON_NUMBER_PARAM,
                    AuxiliarySearchProvider.DEFAULT_FAVICON_NUMBER);

    private static final String USE_LARGE_FAVICON_PARAM = "use_large_favicon";
    public static final BooleanCachedFieldTrialParameter USE_LARGE_FAVICON =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    USE_LARGE_FAVICON_PARAM,
                    false);

    private static final String SCHEDULE_DELAY_TIME_MS_PARAM = "schedule_delay_time_ms";
    public static final IntCachedFieldTrialParameter SCHEDULE_DELAY_TIME_MS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    SCHEDULE_DELAY_TIME_MS_PARAM,
                    AuxiliarySearchProvider.DEFAULT_SCHEDULE_DELAY_TIME_MS);

    @VisibleForTesting static final String SKIP_DEVICE_CHECK_PARAM = "skip_device_check";
    public static final BooleanCachedFieldTrialParameter SKIP_DEVICE_CHECK =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    SKIP_DEVICE_CHECK_PARAM,
                    false);

    /** Convert a Bitmap instance to a byte array. */
    @Nullable
    public static byte[] bitmapToBytes(Bitmap bitmap) {
        if (bitmap == null) return null;

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
        byte[] byteArray = stream.toByteArray();
        bitmap.recycle();
        return byteArray;
    }

    @VisibleForTesting
    public static int getFaviconSize(Resources resources) {
        return USE_LARGE_FAVICON.getValue()
                ? resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size)
                : resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size_small);
    }

    /** Returns the file to save the metadata for donating tabs. */
    @VisibleForTesting
    public static File getTabDonateFile(Context context) {
        return new File(context.getFilesDir(), TAB_DONATE_FILE_NAME);
    }

    /** Returns whether sharing Tabs with the system is enabled in settings. */
    public static boolean isShareTabsWithOsEnabled() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.readBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, true);
    }

    /** Sets whether sharing Tabs with the system is enabled by users. */
    public static void setSharedTabsWithOs(boolean enabled) {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, enabled);
        AuxiliarySearchMetrics.recordIsShareTabsWithOsEnabled(enabled);
    }

    public static void resetSharedTabsWithOsForTesting() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.SHARING_TABS_WITH_OS);
    }
}
