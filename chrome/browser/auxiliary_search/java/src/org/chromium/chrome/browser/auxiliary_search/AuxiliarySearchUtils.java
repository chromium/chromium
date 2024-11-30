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

import java.io.ByteArrayOutputStream;
import java.io.File;

public class AuxiliarySearchUtils {
    @VisibleForTesting static final String TAB_DONATE_FILE_NAME = "tabs_donate";

    // TODO(crbug.com/373902543): Clean up after downstream changes.
    @VisibleForTesting
    static final int DEFAULT_TTL_HOURS =
            ChromeFeatureList.sAndroidAppIntegrationV2ContentTtlHours.getDefaultValue();

    @VisibleForTesting
    static final BooleanCachedFieldTrialParameter SKIP_DEVICE_CHECK =
            ChromeFeatureList.sAndroidAppIntegrationWithFaviconSkipDeviceCheck;

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
        return ChromeFeatureList.sAndroidAppIntegrationWithFaviconUseLargeFavicon.getValue()
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
        return prefsManager.readBoolean(
                ChromePreferenceKeys.SHARING_TABS_WITH_OS,
                AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
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
