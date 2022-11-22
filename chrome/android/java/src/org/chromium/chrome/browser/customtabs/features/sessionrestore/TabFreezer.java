// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;

/**
 * Class used to put tab in freezing state, and remove live references.
 */
class TabFreezer {
    private static final String TAG = "SessionRestore";
    private @Nullable Tab mTab;

    /**
     * Freeze a tab and store it.
     * @param tab Tab to freeze.
     * @return Whether freezing is success
     */
    public boolean freeze(Tab tab) {
        mTab = tab;
        try {
            // TODO(https://crbug.com/1379452): Detach tab with living Java references (e.g.
            // observers).
            // TODO(https://crbug.com/1383325): Freeze the web contents when the API is available.
            mTab.hide(TabHidingType.REPARENTED);

            return true;
        } catch (Exception e) {
            Log.e(TAG, "Store freeze with exception: " + e);
        }
        return false;
    }

    /**
     * Unfreeze the stored tab and pass it.
     * @return The unfrozen Tab.
     */
    public Tab unfreeze() {
        Tab tab = mTab;
        mTab = null;

        return tab;
    }

    /** Return whether a tab is stored in this instance. */
    public boolean hasTab() {
        return mTab != null;
    }

    /**
     * Destroy the tab if exists in the TabFreezer.
     * @return the Tab Id of the cleared tab.
     * */
    public int clear() {
        if (!hasTab()) return Tab.INVALID_TAB_ID;

        int tabId = mTab.getId();
        mTab.destroy();
        mTab = null;
        return tabId;
    }
}
