// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.spdyproxy;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.datareduction.settings.DataReductionDataUseItem;
import org.chromium.chrome.browser.datareduction.settings.DataReductionStatsPreference;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.datareduction.DataReductionProxySavingsClearedReason;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
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

    private static DataReductionProxySettings sSettings;

    // The saved data threshold for showing the Lite mode menu footer.
    private static final long DATA_REDUCTION_MAIN_MENU_ITEM_SAVED_KB_THRESHOLD = 100;

    // The received data threshold for showing the data reduction chart in settings.
    public static final long DATA_REDUCTION_SHOW_CHART_KB_THRESHOLD = 100;

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
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DATA_REDUCTION_ENABLED, enabled);
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
        mNativeDataReductionProxySettings =
                DataReductionProxySettingsJni.get().init(DataReductionProxySettings.this);
    }

    /** Returns true if the SPDY proxy promo is allowed to be shown. */
    public boolean isDataReductionProxyPromoAllowed() {
        return DataReductionProxySettingsJni.get().isDataReductionProxyPromoAllowed(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    /** Returns true if the data saver proxy promo is allowed to be shown as part of FRE. */
    public boolean isDataReductionProxyFREPromoAllowed() {
        return DataReductionProxySettingsJni.get().isDataReductionProxyFREPromoAllowed(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
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
                && SharedPreferencesManager.getInstance().readLong(
                           ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, 0)
                        == 0) {
            SharedPreferencesManager.getInstance().writeLong(
                    ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME,
                    System.currentTimeMillis());
        }
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DATA_REDUCTION_ENABLED, enabled);
        DataReductionProxySettingsJni.get().setDataReductionProxyEnabled(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this, enabled);
    }

    /** Returns true if the Data Reduction Proxy proxy is enabled. */
    public boolean isDataReductionProxyEnabled() {
        return DataReductionProxySettingsJni.get().isDataReductionProxyEnabled(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    /**
     * Returns true if the Data Reduction Proxy menu item should be shown in the main menu.
     */
    public boolean shouldUseDataReductionMainMenuItem() {
        if (!isDataReductionProxyEnabled()) return false;

        return ConversionUtils.bytesToKilobytes(getContentLengthSavedInHistorySummary())
                >= DATA_REDUCTION_MAIN_MENU_ITEM_SAVED_KB_THRESHOLD;
    }

    /** Returns true if the SPDY proxy is managed by an administrator's policy. */
    public boolean isDataReductionProxyManaged() {
        return DataReductionProxySettingsJni.get().isDataReductionProxyManaged(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    /**
     * Returns the time that the data reduction statistics were last updated.
     * @return The last update time in milliseconds since the epoch.
     */
    public long getDataReductionLastUpdateTime() {
        return DataReductionProxySettingsJni.get().getDataReductionLastUpdateTime(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    /**
     * Returns the time that the proxy was first enabled. If data saving statistics are cleared,
     * this is set to the reset time.
     * @return The time that the proxy was first enabled in milliseconds since the epoch.
     */
    public long getDataReductionProxyFirstEnabledTime() {
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, 0);
    }

    /**
     * Clears all data saving statistics.
     * @param reason from the DataReductionProxySavingsClearedReason enum
     */
    public void clearDataSavingStatistics(@DataReductionProxySavingsClearedReason int reason) {
        // When the data saving statistics are cleared, reset the milestone promo that tells the
        // user how much data they have saved using Data Saver so far.
        DataReductionPromoUtils.saveMilestonePromoDisplayed(0);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, System.currentTimeMillis());
        DataReductionProxySettingsJni.get().clearDataSavingStatistics(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this, reason);
    }

    /**
     * Returns aggregate original and received content lengths.
     * @return The content lengths.
     */
    public ContentLengths getContentLengths() {
        return DataReductionProxySettingsJni.get().getContentLengths(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
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
        return DataReductionProxySettingsJni.get().getTotalHttpContentLengthSaved(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    /**
     * Retrieves the history of daily totals of bytes that would have been
     * received if no data reducing mechanism had been applied.
     * @return The history of daily totals
     */
    public long[] getOriginalNetworkStatsHistory() {
        return DataReductionProxySettingsJni.get().getDailyOriginalContentLengths(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    /**
     * Retrieves the history of daily totals of bytes that were received after
     * applying a data reducing mechanism.
     * @return The history of daily totals
     */
    public long[] getReceivedNetworkStatsHistory() {
        return DataReductionProxySettingsJni.get().getDailyReceivedContentLengths(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    /**
     * Determines if the data reduction proxy is currently unreachable.
     * @return true if the data reduction proxy is unreachable.
     */
    public boolean isDataReductionProxyUnreachable() {
        return DataReductionProxySettingsJni.get().isDataReductionProxyUnreachable(
                mNativeDataReductionProxySettings, DataReductionProxySettings.this);
    }

    public Map<String, String> toFeedbackMap() {
        Map<String, String> map = new HashMap<>();
        map.put(DATA_REDUCTION_PROXY_ENABLED_KEY, String.valueOf(isDataReductionProxyEnabled()));
        return map;
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
        DataReductionProxySettingsJni.get().queryDataUsage(mNativeDataReductionProxySettings,
                DataReductionProxySettings.this, new ArrayList<DataReductionDataUseItem>(),
                numDays);
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

    @NativeMethods
    public interface Natives {
        long init(DataReductionProxySettings caller);
        boolean isDataReductionProxyPromoAllowed(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        boolean isDataReductionProxyFREPromoAllowed(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        boolean isDataReductionProxyEnabled(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        boolean isDataReductionProxyManaged(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        void setDataReductionProxyEnabled(long nativeDataReductionProxySettingsAndroid,
                DataReductionProxySettings caller, boolean enabled);
        long getDataReductionLastUpdateTime(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        void clearDataSavingStatistics(long nativeDataReductionProxySettingsAndroid,
                DataReductionProxySettings caller, int reason);
        ContentLengths getContentLengths(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        long getTotalHttpContentLengthSaved(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        long[] getDailyOriginalContentLengths(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        long[] getDailyReceivedContentLengths(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        boolean isDataReductionProxyUnreachable(
                long nativeDataReductionProxySettingsAndroid, DataReductionProxySettings caller);
        void queryDataUsage(long nativeDataReductionProxySettingsAndroid,
                DataReductionProxySettings caller, List<DataReductionDataUseItem> items,
                int numDays);
    }
}
