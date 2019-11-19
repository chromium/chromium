// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import org.chromium.chrome.browser.tab.Tab;

import java.lang.ref.WeakReference;
import java.util.ArrayList;

/**
 * NativePageAssassin tracks recent tabs and freezes each native page when it hasn't been visible
 * for a while. This keeps hidden NativePages from using up precious memory.
 *
 * The NativePageAssassin is a singleton since having full knowledge of the user's hidden tabs --
 * across all local instances of Chrome and all TabModels -- allows the NativePageAssassin to better
 * estimate which hidden tabs the user is likely to return to.
 *
 * Thread safety: this should only be accessed on the UI thread.
 */
public class NativePageAssassin {

    private static final NativePageAssassin sInstance = new NativePageAssassin();

    /**
     * The number of hidden tabs to consider "recent". Any non-recent native page will be frozen.
     */
    private static final int MAX_RECENT_TABS = 3;

    /**
     * The most recently hidden tabs, limited to MAX_RECENT_TABS elements, ordered from oldest to
     * newest. Visible tabs are not included in this list.
     */
    private ArrayList<WeakReference<Tab>> mRecentTabs = new ArrayList<WeakReference<Tab>>(
            MAX_RECENT_TABS + 1);

    private NativePageAssassin() {}

    /**
     * @return The one and only NativePageAssassin.
     */
    public static NativePageAssassin getInstance() {
        return sInstance;
    }

    /**
     * Call this whenever a tab is shown.
     *
     * @param tab The tab being shown.
     */
    public void tabShown(Tab tab) {
        // Remove the tab from the list of recently hidden tabs.
        for (int i = 0; i < mRecentTabs.size(); i++) {
            Tab t = mRecentTabs.get(i).get();
            if (t == tab) {
                mRecentTabs.remove(i);
            }
        }
    }

    /**
     * Call this whenever a tab is hidden.
     *
     * @param tab The tab being hidden.
     */
    public void tabHidden(Tab tab) {
        mRecentTabs.add(new WeakReference<Tab>(tab));

        // If a tab has just passed the threshold from "recent" to "not recent" and it's displaying
        // a native page, freeze the native page.
        if (mRecentTabs.size() <= MAX_RECENT_TABS) return;
        freeze(mRecentTabs.remove(0).get());
    }

    /**
     * Freezes all hidden NativePages that aren't already frozen.
     */
    public void freezeAllHiddenPages() {
        for (int i = 0; i < mRecentTabs.size(); i++) {
            freeze(mRecentTabs.get(i).get());
        }
        mRecentTabs.clear();
    }

    private void freeze(Tab tab) {
        if (tab != null) tab.freezeNativePage();
    }
}
