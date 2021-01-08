// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Helper class for Tabs created from the Start surface.
 */
public class StartSurfaceUserData implements UserData {
    private static final Class<StartSurfaceUserData> USER_DATA_KEY = StartSurfaceUserData.class;
    private boolean mKeepTab;

    /**
     * Sets the flag of whether to keep the given tab in the TabModel without auto deleting when
     * tapping the back button. This flag is for a tab with launchType
     * {@link org.chromium.chrome.browser.tab.TabLaunchType.FROM_START_SURFACE}.
     */
    public static void setKeepTab(Tab tab, boolean keepTab) {
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

    private static StartSurfaceUserData get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }
}
