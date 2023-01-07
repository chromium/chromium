// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;

import java.util.ArrayList;
import java.util.List;

/** Simple mock of TabModel used for tests. */
public class TestTabModel extends EmptyTabModel {
    private final List<Tab> mMockTabs = new ArrayList<>();
    private int mMaxId = -1;
    private int mIndex;

    public void addTab(final String title) {
        mMaxId++;
        final TabImpl mockTab = mock(TabImpl.class);
        final int tabId = mMaxId;
        when(mockTab.getId()).thenReturn(tabId);
        when(mockTab.getTitle()).thenReturn(title);
        mMockTabs.add(mockTab);
    }

    @Override
    public Tab getTabAt(int position) {
        if (position < mMockTabs.size()) {
            return mMockTabs.get(position);
        }
        return null;
    }

    @Override
    public int getCount() {
        return mMockTabs.size();
    }

    @Override
    public int index() {
        return mIndex;
    }

    @Override
    public void closeAllTabs() {
        mMockTabs.clear();
        mMaxId = -1;
        mIndex = 0;
    }

    @Override
    public boolean closeTab(Tab tab, boolean animate, boolean uponExit, boolean canUndo) {
        // The tabId and index are the same.
        mMockTabs.remove(tab.getId());
        return true;
    }

    public void setIndex(int index) {
        mIndex = index;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type, boolean skipLoadingTab) {
        mIndex = i;
    }

    public List<Tab> getAllTabs() {
        return mMockTabs;
    }

    /**
     * Returns the next tab to be selected when a tab is closed.
     * @param id Id of the tab being closed.
     * @param uponExit ignored.
     * @return The next tab if available or null.
     */
    @Override
    public Tab getNextTabIfClosed(int id, boolean uponExit) {
        if (id > 0 && id < mMockTabs.size()) {
            return mMockTabs.get(id - 1);
        } else if (id == 0 && mMockTabs.size() > 1) {
            return mMockTabs.get(1);
        }
        return null;
    }
}