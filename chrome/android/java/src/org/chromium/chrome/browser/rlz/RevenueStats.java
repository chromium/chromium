// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.rlz;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;

/** Utility class for managing revenue sharing information. */
@JNINamespace("chrome::android")
public class RevenueStats {
    private static RevenueStats sInstance;

    /** Returns the singleton instance of ExternalAuthUtils, creating it if needed. */
    public static RevenueStats getInstance() {
        assert ThreadUtils.runningOnUiThread();
        if (sInstance == null) {
            sInstance = AppHooks.get().createRevenueStatsInstance();
        }

        return sInstance;
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

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void setSearchClient(@JniType("std::string") String client);

        void setRlzParameterValue(@JniType("std::u16string") String rlz);
    }
}
