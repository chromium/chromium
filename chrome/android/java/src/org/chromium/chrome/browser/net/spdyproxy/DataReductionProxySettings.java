// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.spdyproxy;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionDataUseItem;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionProxySavingsClearedReason;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionStatsPreference;
import org.chromium.chrome.browser.util.ConversionUtils;

import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Entry point to manage all data reduction proxy configuration details.
 */
public class DataReductionProxySettings {

    /**
     * Data structure to hold the original content length before data reduction and the received
     * content length after data reduction.
     */
    public static class ContentLengths {
        private final long mOriginal;
        private final long mReceived;

        @CalledByNative("ContentLengths")
        public static ContentLengths create(long original, long received) {
            return new ContentLengths(original, received);
        }

        private ContentLengths(long original, long received) {
            mOriginal = original;
            mReceived = received;
        }

        public long getOriginal() {
            return mOriginal;
        }

        public long getReceived() {
            return mReceived;
        }
    }

    @VisibleForTesting
    public static final String DATA_REDUCTION_PROXY_ENABLED_KEY = "Data Reduction Proxy Enabled";

    // Visible for backup and restore
    public static final String DATA_REDUCTION_ENABLED_PREF = "BANDWIDTH_REDUCTION_PROXY_ENABLED";

    private static DataReductionProxySettings sSettings;

    private static final String DATA_REDUCTION_HAS_EVER_BEEN_ENABLED_PREF =
            "BANDWIDTH_REDUCTION_PROXY_HAS_EVER_BEEN_ENABLED";
    public static final String DATA_REDUCTION_FIRST_ENABLED_TIME =
            "BANDWIDTH_REDUCTION_FIRST_ENABLED_TIME";

    private static final String PARAM_PERSISTENT_MENU_ITEM_ENABLED = "persistent_menu_item_enabled";

    private static final long DATA_REDUCTION_MAIN_MENU_ITEM_SAVED_KB_THRESHOLD = 100;

    private Callback<List<DataReductionDataUseItem>> mQueryDataUsageCallback;

    /**
     * Handles calls for data reduction proxy initialization that need to happen after the native
     * library has been loaded.
     */
    public static void handlePostNativeInitialization() {
        reconcileDataReductionProxyEnabledState();
        DataReductionStatsPreference.initializeDataReductionSiteBreakdownPref();
    }

    /**
     * Reconciles the Java-side data reduction proxy state with the native one.
     *
     * The data reduction proxy state needs to be accessible before the native
     * library has been loaded, from Java. This is possible through
     * isEnabledBeforeNativeLoad(). Once the native library has been loaded, the
     * Java preference has to be updated.
     * This method must be called early at startup, but once the native library
     * has been loaded.
     */
    private static void reconcileDataReductionProxyEnabledState() {
        ThreadUtils.assertOnUiThread();
        boolean enabled = getInstance().isDataReductionProxyEnabled();
        ContextUtils.getAppSharedPreferences().edit()
                .putBoolean(DATA_REDUCTION_ENABLED_PREF, enabled).apply();
    }

    /**
     * Returns a singleton instance of the settings object.
     *
     * Needs the native library to be loaded, otherwise it will crash.
     */
    public static DataReductionProxySettings getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sSettings == null) {
            sSettings = new DataReductionProxySettings();
        }
        return sSettings;
    }

    /**
     * Sets a singleton instance of the settings object for testing.
     */
    @VisibleForTesting
    public static void setInstanceForTesting(DataReductionProxySettings settings) {
        sSettings = settings;
    }

    private final long mNativeDataReductionProxySettings;

    protected DataReductionProxySettings() {
        // Note that this technically leaks the native object, however,
        // DataReductionProxySettings is a singleton that lives forever and there's no clean
        // shutdown of Chrome on Android
        mNativeDataReductionProxySettings = nativeInit();
    }

    /** Returns true if the SPDY proxy promo is allowed to be shown. */
    public boolean isDataReductionProxyPromoAllowed() {
        return nativeIsDataReductionProxyPromoAllowed(mNativeDataReductionProxySettings);
    }

    /** Returns true if the data saver proxy promo is allowed to be shown as part of FRE. */
    public boolean isDataReductionProxyFREPromoAllowed() {
        return nativeIsDataReductionProxyFREPromoAllowed(mNativeDataReductionProxySettings);
    }

    /** Returns true if the snackbar promo is allowed to be shown. */
    public boolean isSnackbarPromoAllowed(String url) {
        return url.startsWith(UrlConstants.HTTP_URL_PREFIX) && isDataReductionProxyEnabled();
    }

    /**
     * Sets the preference on whether to enable/disable the SPDY proxy. This will zero out the
     * data reduction statistics if this is the first time the SPDY proxy has been enabled.
     */
    public void setDataReductionProxyEnabled(Context context, boolean enabled) {
        if (enabled
                && ContextUtils.getAppSharedPreferences().getLong(
                           DATA_REDUCTION_FIRST_ENABLED_TIME, 0)
                        == 0) {
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putLong(DATA_REDUCTION_FIRST_ENABLED_TIME, System.currentTimeMillis())
                    .apply();
        }
        ContextUtils.getAppSharedPreferences().edit()
                .putBoolean(DATA_REDUCTION_ENABLED_PREF, enabled).apply();
        nativeSetDataReductionProxyEnabled(mNativeDataReductionProxySettings, enabled);
    }

    /** Returns true if the Data Reduction Proxy proxy is enabled. */
    public boolean isDataReductionProxyEnabled() {
        return nativeIsDataReductionProxyEnabled(mNativeDataReductionProxySettings);
    }

    /**
     * Returns true if the Data Reduction Proxy menu item should be shown in the main menu.
     */
    public boolean shouldUseDataReductionMainMenuItem() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_REDUCTION_MAIN_MENU)) return false;

        boolean data_reduction_main_menu_item_allowed = false;
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.DATA_REDUCTION_MAIN_MENU, PARAM_PERSISTENT_MENU_ITEM_ENABLED,
                    false)) {
            // If the Data Reduction Proxy is enabled, set the pref storing that the proxy has
            // ever been enabled.
            if (isDataReductionProxyEnabled()) {
                ContextUtils.getAppSharedPreferences()
                        .edit()
                        .putBoolean(DATA_REDUCTION_HAS_EVER_BEEN_ENABLED_PREF, true)
                        .apply();
            }
            data_reduction_main_menu_item_allowed =
                    ContextUtils.getAppSharedPreferences().getBoolean(
                            DATA_REDUCTION_HAS_EVER_BEEN_ENABLED_PREF, false);
        } else {
            data_reduction_main_menu_item_allowed = isDataReductionProxyEnabled();
        }

        if (data_reduction_main_menu_item_allowed) {
            return ConversionUtils.bytesToKilobytes(getContentLengthSavedInHistorySummary())
                    >= DATA_REDUCTION_MAIN_MENU_ITEM_SAVED_KB_THRESHOLD;
        }
        return false;
    }

    /** Returns true if the SPDY proxy is managed by an administrator's policy. */
    public boolean isDataReductionProxyManaged() {
        return nativeIsDataReductionProxyManaged(mNativeDataReductionProxySettings);
    }

    /**
     * Returns the time that the data reduction statistics were last updated.
     * @return The last update time in milliseconds since the epoch.
     */
    public long getDataReductionLastUpdateTime() {
        return nativeGetDataReductionLastUpdateTime(mNativeDataReductionProxySettings);
    }

    /**
     * Returns the time that the proxy was first enabled. If data saving statistics are cleared,
     * this is set to the reset time.
     * @return The time that the proxy was first enabled in milliseconds since the epoch.
     */
    public long getDataReductionProxyFirstEnabledTime() {
        return ContextUtils.getAppSharedPreferences().getLong(DATA_REDUCTION_FIRST_ENABLED_TIME, 0);
    }

    /**
     * Clears all data saving statistics.
     * @param reason from the DataReductionProxySavingsClearedReason enum
     */
    public void clearDataSavingStatistics(@DataReductionProxySavingsClearedReason int reason) {
        // When the data saving statistics are cleared, reset the snackbar promo that tells the user
        // how much data they have saved using Data Saver so far.
        DataReductionPromoUtils.saveSnackbarPromoDisplayed(0);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(DATA_REDUCTION_FIRST_ENABLED_TIME, System.currentTimeMillis())
                .apply();
        nativeClearDataSavingStatistics(mNativeDataReductionProxySettings, reason);
    }

    /**
     * Returns aggregate original and received content lengths.
     * @return The content lengths.
     */
    public ContentLengths getContentLengths() {
        return nativeGetContentLengths(mNativeDataReductionProxySettings);
    }

    /**
     * Returns the content length saved for the number of days shown in the history summary.
     * @return The content length saved.
     */
    public long getContentLengthSavedInHistorySummary() {
        ContentLengths length = getContentLengths();
        return Math.max(length.getOriginal() - length.getReceived(), 0);
    }

    /**
     * Returns the total HTTP content length saved.
     * @return The HTTP content length saved.
     */
    public long getTotalHttpContentLengthSaved() {
        return nativeGetTotalHttpContentLengthSaved(mNativeDataReductionProxySettings);
    }

    /**
     * Retrieves the history of daily totals of bytes that would have been
     * received if no data reducing mechanism had been applied.
     * @return The history of daily totals
     */
    public long[] getOriginalNetworkStatsHistory() {
        return nativeGetDailyOriginalContentLengths(mNativeDataReductionProxySettings);
    }

    /**
     * Retrieves the history of daily totals of bytes that were received after
     * applying a data reducing mechanism.
     * @return The history of daily totals
     */
    public long[] getReceivedNetworkStatsHistory() {
        return nativeGetDailyReceivedContentLengths(mNativeDataReductionProxySettings);
    }

    /**
     * Returns the header used to request a data reduction proxy pass through. When a request is
     * sent to the data reduction proxy with this header, it will respond with the original
     * uncompressed response.
     * @return The data reduction proxy pass through header.
     */
    public String getDataReductionProxyPassThroughHeader() {
        return nativeGetDataReductionProxyPassThroughHeader(mNativeDataReductionProxySettings);
    }

    /**
     * Determines if the data reduction proxy is currently unreachable.
     * @return true if the data reduction proxy is unreachable.
     */
    public boolean isDataReductionProxyUnreachable() {
        return nativeIsDataReductionProxyUnreachable(mNativeDataReductionProxySettings);
    }

    /**
     * @return The data reduction settings as a string percentage.
     */
    public String getContentLengthPercentSavings() {
        ContentLengths length = getContentLengths();

        double savings = 0;
        if (length.getOriginal() > 0L  && length.getOriginal() > length.getReceived()) {
            savings = (length.getOriginal() - length.getReceived()) / (double) length.getOriginal();
        }
        NumberFormat percentageFormatter = NumberFormat.getPercentInstance(Locale.getDefault());
        return percentageFormatter.format(savings);
    }

    public Map<String, String> toFeedbackMap() {
        Map<String, String> map = new HashMap<>();
        map.put(DATA_REDUCTION_PROXY_ENABLED_KEY, String.valueOf(isDataReductionProxyEnabled()));
        return map;
    }

    /**
     * If the given URL is a WebLite URL and should be overridden because the Data
     * Reduction Proxy is on, the user is in the Lo-Fi previews experiment, and the scheme of the
     * lite_url param is HTTP, returns the URL contained in the lite_url param. Otherwise returns
     * the given URL.
     *
     * @param url The URL to evaluate.
     * @return The URL to be used. Returns null if the URL param is null.
     */
    public String maybeRewriteWebliteUrl(String url) {
        return nativeMaybeRewriteWebliteUrl(mNativeDataReductionProxySettings, url);
    }

    /**
     * Queries native Data Reduction Proxy to get data use statistics. On query completion provides
     * a list of DataReductionDataUseItem to the callback.
     *
     * @param numDays Number of days to get stats for.
     * @param queryDataUsageCallback Callback to give the list of DataReductionDataUseItems on query
     *            completion.
     */
    public void queryDataUsage(
            int numDays, Callback<List<DataReductionDataUseItem>> queryDataUsageCallback) {
        mQueryDataUsageCallback = queryDataUsageCallback;
        nativeQueryDataUsage(mNativeDataReductionProxySettings,
                new ArrayList<DataReductionDataUseItem>(), numDays);
    }

    @CalledByNative
    public static void createDataUseItemAndAddToList(List<DataReductionDataUseItem> items,
            String hostname, long dataUsed, long originalSize) {
        items.add(new DataReductionDataUseItem(hostname, dataUsed, originalSize));
    }

    @CalledByNative
    public void onQueryDataUsageComplete(List<DataReductionDataUseItem> items) {
        if (mQueryDataUsageCallback != null) {
            mQueryDataUsageCallback.onResult(items);
        }
        mQueryDataUsageCallback = null;
    }

    private native long nativeInit();
    private native boolean nativeIsDataReductionProxyPromoAllowed(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyFREPromoAllowed(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyEnabled(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyManaged(
            long nativeDataReductionProxySettingsAndroid);
    private native void nativeSetDataReductionProxyEnabled(
            long nativeDataReductionProxySettingsAndroid, boolean enabled);
    private native long nativeGetDataReductionLastUpdateTime(
            long nativeDataReductionProxySettingsAndroid);
    private native void nativeClearDataSavingStatistics(
            long nativeDataReductionProxySettingsAndroid, int reason);
    private native ContentLengths nativeGetContentLengths(
            long nativeDataReductionProxySettingsAndroid);
    private native long nativeGetTotalHttpContentLengthSaved(
            long nativeDataReductionProxySettingsAndroid);
    private native long[] nativeGetDailyOriginalContentLengths(
            long nativeDataReductionProxySettingsAndroid);
    private native long[] nativeGetDailyReceivedContentLengths(
            long nativeDataReductionProxySettingsAndroid);
    private native String nativeGetDataReductionProxyPassThroughHeader(
            long nativeDataReductionProxySettingsAndroid);
    private native boolean nativeIsDataReductionProxyUnreachable(
            long nativeDataReductionProxySettingsAndroid);
    private native String nativeMaybeRewriteWebliteUrl(
            long nativeDataReductionProxySettingsAndroid, String url);
    private native void nativeQueryDataUsage(long nativeDataReductionProxySettingsAndroid,
            List<DataReductionDataUseItem> items, int numDays);
}
