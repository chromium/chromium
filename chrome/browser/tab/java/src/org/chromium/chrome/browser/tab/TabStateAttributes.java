// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;

/**
 * Attributes related to {@link TabState}
 */
public class TabStateAttributes implements UserData {
    private static final Class<TabStateAttributes> USER_DATA_KEY = TabStateAttributes.class;
    /** Whether or not the TabState has changed. */
    private boolean mIsTabStateDirty = true;
    private Tab mTab;

    /**
     * @return {@link TabStateAttributes} for a {@link Tab}
     */
    public static TabStateAttributes from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        TabStateAttributes attrs = host.getUserData(USER_DATA_KEY);
        return attrs != null ? attrs : host.setUserData(USER_DATA_KEY, new TabStateAttributes(tab));
    }

    private TabStateAttributes(Tab tab) {
        mTab = tab;
    }

    /**
     * @return true if the {@link TabState} has been changed
     */
    public boolean isTabStateDirty() {
        return mIsTabStateDirty;
    }

    /**
     * Signals that the tab state is dirty and needs to be persisted.
     */
    public void markTabStateDirty() {
        mIsTabStateDirty = true;
        if (!mTab.isDestroyed()) CriticalPersistedTabData.from(mTab).setShouldSave();
    }

    /**
     * Signals that the tab state is no longer dirty (e.g. has been successfully persisted).
     */
    public void clearTabStateDirtiness() {
        mIsTabStateDirty = false;
    }
}
