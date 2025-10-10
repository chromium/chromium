// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.rlz;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;

/** Utility class for managing revenue sharing information. */
@JNINamespace("chrome::android")
@NullMarked
public class RevenueStats {
    private static @Nullable RevenueStats sInstance;
    private static @Nullable Callback<@Nullable String> sSetCustomTabSearchClientHookForTesting;

    /** Returns the singleton instance of ExternalAuthUtils, creating it if needed. */
    public static RevenueStats getInstance() {
        assert ThreadUtils.runningOnUiThread();
        if (sInstance == null) {
            RevenueStats instance = ServiceLoaderUtil.maybeCreate(RevenueStats.class);
            if (instance == null) {
                instance = new RevenueStats();
            }
            sInstance = instance;
        }

        return sInstance;
    }

    public static void setInstanceForTesting(RevenueStats instance) {
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = null);
    }

    /** Notifies tab creation event. */
    public void tabCreated(Tab tab) {}

    /** Read and apply RLZ and ClientID values. */
    public void retrieveAndApplyTrackingIds() {}

    /** Returns whether the RLZ provider has been notified that the first search has occurred. */
    protected static boolean getRlzNotified() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.RLZ_NOTIFIED, false);
    }

    /** Stores that the RLZ provider has been notified that the first search has occurred. */
    protected static void markRlzNotified() {
        ChromeSharedPreferences.getInstance().writeBoolean(ChromePreferenceKeys.RLZ_NOTIFIED, true);
    }

    /** Sets search client id. */
    protected static void setSearchClient(String client) {
        RevenueStatsJni.get().setSearchClient(client);
    }

    /** Sets rlz value. */
    protected static void setRlzParameterValue(String rlz) {
        RevenueStatsJni.get().setRlzParameterValue(rlz);
    }

    /**
     * Specify SearchClient to use within the Chrome Custom Tab session.
     *
     * <p>A non-null value will override any value specified by {@link setSearchClient(String)}, so
     * this should only be called when initiating a search from a Custom Tab instance.
     *
     * @param client the client value to use, or null to reset.
     */
    public static void setCustomTabSearchClient(@Nullable String client) {
        if (sSetCustomTabSearchClientHookForTesting != null) {
            sSetCustomTabSearchClientHookForTesting.onResult(client);
            return;
        }
        RevenueStatsJni.get().setCustomTabSearchClient(client);
    }

    public static void setCustomTabSearchClientHookForTesting(Callback<@Nullable String> hook) {
        sSetCustomTabSearchClientHookForTesting = hook;
        ResettersForTesting.register(() -> sSetCustomTabSearchClientHookForTesting = null);
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void setSearchClient(@JniType("std::string") String client);

        void setCustomTabSearchClient(@Nullable String client);

        void setRlzParameterValue(@JniType("std::u16string") String rlz);
    }
}
