// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.rlz;

import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Utility class for managing revenue sharing information.
 */
@JNINamespace("chrome::android")
public class RevenueStats {
    private static final String PREF_RLZ_NOTIFIED = "rlz_first_search_notified";

    private static RevenueStats sInstance;

    /**
     * Returns the singleton instance of ExternalAuthUtils, creating it if needed.
     */
    public static RevenueStats getInstance() {
        assert ThreadUtils.runningOnUiThread();
        if (sInstance == null) {
            sInstance = AppHooks.get().createRevenueStatsInstance();
        }

        return sInstance;
    }

    /**
     * Notifies tab creation event.
     */
    public void tabCreated(Tab tab) {}

    /**
     * Returns whether the RLZ provider has been notified that the first search has occurred.
     */
    protected static boolean getRlzNotified() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_RLZ_NOTIFIED, false);
    }

    /**
     * Stores whether the RLZ provider has been notified that the first search has occurred as
     * shared preference.
     */
    protected static void setRlzNotified(boolean notified) {
        SharedPreferences.Editor sharedPreferencesEditor =
                ContextUtils.getAppSharedPreferences().edit();
        sharedPreferencesEditor.putBoolean(PREF_RLZ_NOTIFIED, notified);
        sharedPreferencesEditor.apply();
    }

    /**
     * Sets search client id.
     */
    protected static void setSearchClient(String client) {
        RevenueStatsJni.get().setSearchClient(client);
    }

    /**
     * Sets rlz value.
     */
    protected static void setRlzParameterValue(String rlz) {
        RevenueStatsJni.get().setRlzParameterValue(rlz);
    }

    @NativeMethods
    interface Natives {
        void setSearchClient(String client);
        void setRlzParameterValue(String rlz);
    }
}
