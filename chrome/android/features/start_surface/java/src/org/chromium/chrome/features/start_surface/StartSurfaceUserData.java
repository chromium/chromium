// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;

/**
 * Helper class for Tabs created from the Start surface.
 */
public class StartSurfaceUserData implements UserData {
    private static final Class<StartSurfaceUserData> USER_DATA_KEY = StartSurfaceUserData.class;
    private boolean mKeepTab;
    private boolean mOpenedFromStart;
    // Saves the Feeds instance state.
    private String mFeedsInstanceState;
    /**
     * Tracks whether the last visited Tab is restored at startup but not showing due to the
     * overview page is showing at the startup.
     */
    private boolean mUnusedTabRestoredAtStartup;

    /**
     * Static class that implements the initialization-on-demand holder idiom.
     */
    private static class LazyHolder {
        static final StartSurfaceUserData INSTANCE = new StartSurfaceUserData();
    }

    /**
     * Gets the singleton instance for the StartSurfaceUserData.
     */
    public static StartSurfaceUserData getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Sets the flag of whether to keep the given tab in the TabModel without auto deleting when
     * tapping the back button. This flag is for a tab with launchType
     * {@link org.chromium.chrome.browser.tab.TabLaunchType.FROM_START_SURFACE}.
     */
    public static void setKeepTab(Tab tab, boolean keepTab) {
        if (tab == null || tab.getLaunchType() != TabLaunchType.FROM_START_SURFACE) return;

        StartSurfaceUserData startSurfaceUserData = get(tab);
        if (startSurfaceUserData == null) {
            startSurfaceUserData = new StartSurfaceUserData();
        }
        startSurfaceUserData.mKeepTab = keepTab;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, startSurfaceUserData);
    }

    /**
     * @return Whether to keep the given tab in the TabModel without auto deleting when tapping the
     * back button. Returns false if the UserData isn't set.
     */
    public static boolean getKeepTab(Tab tab) {
        StartSurfaceUserData startSurfaceUserData = get(tab);
        return startSurfaceUserData == null ? false : startSurfaceUserData.mKeepTab;
    }

    /**
     * Sets the flag of whether the given tab is opened from the Start surface. Note: should only
     * call this function in the code path that Start surface is enabled, otherwise may cause the
     * StartSurfaceUserData is created without Start surface.
     */
    public static void setOpenedFromStart(Tab tab) {
        if (tab == null || !StartSurfaceConfiguration.isStartSurfaceFlagEnabled()) return;

        StartSurfaceUserData startSurfaceUserData = get(tab);
        if (startSurfaceUserData == null) {
            startSurfaceUserData = new StartSurfaceUserData();
        }

        if (startSurfaceUserData.mOpenedFromStart) return;

        startSurfaceUserData.mOpenedFromStart = true;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, startSurfaceUserData);
    }

    /**
     * @return Whether the given tab is opened from the Start surface.
     */
    public static boolean isOpenedFromStart(Tab tab) {
        StartSurfaceUserData startSurfaceUserData = get(tab);
        return startSurfaceUserData == null ? false : startSurfaceUserData.mOpenedFromStart;
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
    @Nullable
    protected String restoreFeedInstanceState() {
        return mFeedsInstanceState;
    }

    /**
     * Sets whether an unused Tab is restored at startup due to an overview page is showing at the
     * startup.
     */
    public void setUnusedTabRestoredAtStartup(boolean overviewShownAtStartup) {
        mUnusedTabRestoredAtStartup = overviewShownAtStartup;
    }

    /**
     * Gets whether an unused Tab is restored at startup due to an overview page is showing at the
     * startup.
     */
    public boolean getUnusedTabRestoredAtStartup() {
        return mUnusedTabRestoredAtStartup;
    }
}
