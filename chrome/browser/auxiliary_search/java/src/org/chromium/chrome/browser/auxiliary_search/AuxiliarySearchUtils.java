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
    @VisibleForTesting static final int MODULE_SHOWN_MAX_IMPRESSIONS = 3;

    // TODO(crbug.com/373902543): Clean up after downstream changes.
    @VisibleForTesting
    static final int DEFAULT_TTL_HOURS =
            ChromeFeatureList.sAndroidAppIntegrationV2ContentTtlHours.getDefaultValue();

    @VisibleForTesting
    static final BooleanCachedFieldTrialParameter SKIP_DEVICE_CHECK =
            ChromeFeatureList.sAndroidAppIntegrationWithFaviconSkipDeviceCheck;

    @VisibleForTesting
    static final BooleanCachedFieldTrialParameter FORCE_CARD_SHOWN =
            ChromeFeatureList.sAndroidAppIntegrationWithFaviconForceCardShow;

    @VisibleForTesting
    static final BooleanCachedFieldTrialParameter SHOW_THIRD_PARTY_CARD =
            ChromeFeatureList.sAndroidAppIntegrationWithFaviconShowThirdPartyCard;

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

    /**
     * Increments the module impression counter.
     *
     * @return The impression counter incremented by 1.
     */
    public static int incrementModuleImpressions() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        return prefsManager.incrementInt(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION);
    }

    /**
     * Returns whether the user has clicked the opt-in or opt-out button on the auxiliary search
     * module card to response.
     */
    public static boolean hasUserResponded() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED, false);
    }

    /**
     * Returns whether the auxiliary search module card has exceeded the maximum allowed
     * impressions.
     */
    public static boolean exceedMaxImpressions() {
        return ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION, 0)
                >= MODULE_SHOWN_MAX_IMPRESSIONS;
    }

    /**
     * Returns whether the auxiliary search module card can be shown. The card can only be shown if
     * 1) it hasn't been shown in the current session; 2) the user hasn't responded by clicking any
     * button on the card; 3) it hasn't exceeded the allowed maximum impressions. The function skips
     * all checks if AuxiliarySearchUtils.FORCE_CARD_SHOWN is enabled.
     *
     * @param shownInThisSession Whether the card has been shown once in the current session.
     */
    public static boolean canShowCard(Boolean shownInThisSession) {
        if (AuxiliarySearchUtils.FORCE_CARD_SHOWN.getValue()) return true;

        boolean hasShown = shownInThisSession == null ? false : shownInThisSession.booleanValue();
        return !hasShown
                && !AuxiliarySearchUtils.hasUserResponded()
                && !AuxiliarySearchUtils.exceedMaxImpressions();
    }

    /** Returns whether the sharing Tabs settings is enabled by default. */
    public static boolean isShareTabsWithOsDefaultEnabled() {
        return AuxiliarySearchUtils.SKIP_DEVICE_CHECK.getValue()
                ? !AuxiliarySearchUtils.SHOW_THIRD_PARTY_CARD.getValue()
                : AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs();
    }

    public static void resetSharedPreferenceForTesting() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.SHARING_TABS_WITH_OS);
        prefsManager.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION);
        prefsManager.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED);
    }
}
