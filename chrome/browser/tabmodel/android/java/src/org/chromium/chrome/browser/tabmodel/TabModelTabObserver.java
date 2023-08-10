// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * Observer of tab changes for all tabs owned by a {@link TabModel}.
 */
public class TabModelTabObserver extends EmptyTabObserver {
    private final TabModel mTabModel;
    private final TabModelObserver mTabModelObserver;

    /**
     * Constructs an observer that should be notified of tab changes for all tabs
     * owned by a specified {@link TabModel}. Any Tabs created after this call will
     * be observed as well, and Tabs removed will no longer have their information
     * broadcast.
     *
     * <p>
     * {@link #destroy()} must be called to unregister this observer.
     *
     * @param model The model that owns the Tabs that should notify this observer.
     */
    public TabModelTabObserver(TabModel model) {
        mTabModel = model;
        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                onTabSelected(tab);
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                tab.removeObserver(TabModelTabObserver.this);
            }

            @Override
            public void restoreCompleted() {
                maybeCallOnTabSelected();
            }
        };

        mTabModel.addObserver(mTabModelObserver);
        maybeCallOnTabSelected();
    }

    /**
     * Destroys the observer and removes itself as a listener for Tab updates.
     */
    public void destroy() {
        mTabModel.removeObserver(mTabModelObserver);
        int tabCount = mTabModel.getCount();
        for (int i = 0; i < tabCount; i++) {
            mTabModel.getTabAt(i).removeObserver(this);
        }
    }

    /* Called when a tab in a model is selected or restored. */
    protected void onTabSelected(Tab tab) {
        tab.addObserver(TabModelTabObserver.this);
    };

    private void maybeCallOnTabSelected() {
        if (mTabModel.index() != TabList.INVALID_TAB_INDEX) {
            onTabSelected(mTabModel.getTabAt(mTabModel.index()));
        }
    }
}
