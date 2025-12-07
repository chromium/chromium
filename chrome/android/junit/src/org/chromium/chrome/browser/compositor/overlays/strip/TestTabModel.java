// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabRemover;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/** Simple mock of TabModel used for tests. */
public class TestTabModel extends EmptyTabModel {
    private final List<Tab> mMockTabs = new ArrayList<>();
    private final Set<Integer> mMultiSelectedTabs = new HashSet<>();
    private @Nullable TabRemover mTabRemover;
    private int mMaxId = -1;
    private int mIndex;

    public void addTab(final String title) {
        mMaxId++;
        final Tab mockTab = mock(Tab.class);
        final int tabId = mMaxId;
        when(mockTab.getId()).thenReturn(tabId);
        when(mockTab.getTitle()).thenReturn(title);
        mMockTabs.add(mockTab);
    }

    @Override
    public TabRemover getTabRemover() {
        assert mTabRemover != null;
        return mTabRemover;
    }

    @Override
    public Tab getTabAt(int position) {
        if (position < 0 || position >= mMockTabs.size()) return null;
        return mMockTabs.get(position);
    }

    @Override
    public @Nullable Tab getTabById(int tabId) {
        return mMockTabs.stream().filter(t -> t.getId() == tabId).findAny().orElse(null);
    }

    @Override
    public int getCount() {
        return mMockTabs.size();
    }

    @Override
    public Iterator<Tab> iterator() {
        return mMockTabs.iterator();
    }

    @Override
    public int index() {
        return mIndex;
    }

    @Override
    public boolean closeTabs(TabClosureParams params) {
        if (params.isAllTabs) {
            mMockTabs.clear();
            mMaxId = -1;
            mIndex = 0;
        } else {
            for (Tab tab : params.tabs) {
                mMockTabs.remove(tab.getId());
            }
        }
        return true;
    }

    public void setIndex(int index) {
        mIndex = index;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type) {
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

    @Override
    public int indexOf(@Nullable Tab tab) {
        return mMockTabs.indexOf(tab);
    }

    public void setTabRemover(TabRemover tabRemover) {
        mTabRemover = tabRemover;
    }

    @Override
    public void setTabsMultiSelected(Set<Integer> tabIds, boolean isSelected) {
        if (isSelected) {
            mMultiSelectedTabs.addAll(tabIds);
        } else {
            mMultiSelectedTabs.removeAll(tabIds);
        }
    }

    @Override
    public void clearMultiSelection(boolean notifyObservers) {
        if (mMultiSelectedTabs.isEmpty()) return;
        mMultiSelectedTabs.clear();
    }

    @Override
    public boolean isTabMultiSelected(int tabId) {
        return mMultiSelectedTabs.contains(tabId) || tabId == TabModelUtils.getCurrentTabId(this);
    }

    @Override
    public int getMultiSelectedTabsCount() {
        if (mMockTabs.isEmpty()) return 0;
        // If no other tabs are in multi-selection, this returns 1, as the active tab is always
        // considered selected.
        return mMultiSelectedTabs.isEmpty() ? 1 : mMultiSelectedTabs.size();
    }
}
