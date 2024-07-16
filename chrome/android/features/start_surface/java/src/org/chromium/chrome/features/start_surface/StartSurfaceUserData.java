// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.Tab;

/** Helper class for Tabs created from the Start surface. */
public class StartSurfaceUserData implements UserData {
    private static final Class<StartSurfaceUserData> USER_DATA_KEY = StartSurfaceUserData.class;
    // Saves the Feeds instance state.
    private String mFeedsInstanceState;

    /**
     * Tracks whether the last visited Tab is restored at startup but not showing due to the
     * overview page is showing at the startup.
     */
    private boolean mUnusedTabRestoredAtStartup;

    // Whether the singleton instance has been created.
    private static boolean sHasInstance;

    private StartSurfaceUserData() {
        sHasInstance = true;
    }

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final StartSurfaceUserData INSTANCE = new StartSurfaceUserData();
    }

    /** Gets the singleton instance for the StartSurfaceUserData. */
    public static StartSurfaceUserData getInstance() {
        return LazyHolder.INSTANCE;
    }

    /** Cleans up any state which should be reset when recreating the ChromeTabbedActivity. */
    public static void reset() {
        if (sHasInstance) {
            getInstance().saveFeedInstanceState(null);
        }
    }

    private static StartSurfaceUserData get(Tab tab) {
        if (tab.isDestroyed()) return null;

        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /** Save the feed instance state if necessary. */
    public void saveFeedInstanceState(String state) {
        mFeedsInstanceState = state;
    }

    /**
     * @return The saved feed instance state, or null if it is not previously saved.
     */
    protected @Nullable String restoreFeedInstanceState() {
        return mFeedsInstanceState;
    }

    /**
     * Sets whether an unused Tab is restored at startup due to an overview page is showing at the
     * startup.
     */
    public void setUnusedTabRestoredAtStartup(boolean overviewShownAtStartup) {
        mUnusedTabRestoredAtStartup = overviewShownAtStartup;
    }

    static boolean hasInstanceForTesting() {
        return sHasInstance;
    }
}
