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
    private boolean mFocusOnOmnibox;
    private boolean mCreatedAsNtp;

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

    /**
     * Sets whether to focus on omnibox when the given tab is shown.
     */
    public static void setFocusOnOmnibox(Tab tab, boolean focusOnOmnibox) {
        StartSurfaceUserData startSurfaceUserData = get(tab);
        if (startSurfaceUserData == null) {
            startSurfaceUserData = new StartSurfaceUserData();
            tab.getUserDataHost().setUserData(USER_DATA_KEY, startSurfaceUserData);
        }
        startSurfaceUserData.mFocusOnOmnibox = focusOnOmnibox;
    }

    /**
     * @return Whether to focus on omnibox when the given tab is shown. The focusing on omnibox will
     * only shown when the tab is created as a new Tab.
     */
    public static boolean getFocusOnOmnibox(Tab tab) {
        StartSurfaceUserData startSurfaceUserData = get(tab);
        return startSurfaceUserData == null ? false : startSurfaceUserData.mFocusOnOmnibox;
    }

    private static StartSurfaceUserData get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Sets whether the tab is created as chrome://newTab. A tab can only be created in this way
     * when {@link StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled. The URL of the
     * newly created tab is empty, but should be treated as NTP for features like autocomplete.
     */
    public static void setCreatedAsNtp(Tab tab) {
        assert StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB.getValue();

        StartSurfaceUserData startSurfaceUserData = get(tab);
        if (startSurfaceUserData == null) {
            startSurfaceUserData = new StartSurfaceUserData();
            tab.getUserDataHost().setUserData(USER_DATA_KEY, startSurfaceUserData);
        }
        startSurfaceUserData.mCreatedAsNtp = true;
    }

    /**
     * @return Whether the tab is created as chrome://newTab. A tab can only be created in this way
     * when {@link StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled. The URL of the
     * newly created tab is empty, but should be treated as NTP for features like autocomplete.
     */
    public static boolean getCreatedAsNtp(Tab tab) {
        StartSurfaceUserData startSurfaceUserData = get(tab);
        return startSurfaceUserData == null ? false : startSurfaceUserData.mCreatedAsNtp;
    }
}
