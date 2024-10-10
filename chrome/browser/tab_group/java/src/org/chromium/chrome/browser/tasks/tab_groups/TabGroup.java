// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * This class is a representation of a group of tabs. It knows the last selected tab within the
 * group.
 *
 * <p>Note that this class is scoped to the "tab_groups" package. It is used internally by {@link
 * TabGroupModelFilter} and any lookups for tab groups should go through that filter.
 */
class TabGroup {
    static final int INVALID_ROOT_ID = -1;
    static final int INVALID_POSITION_IN_GROUP = -1;

    private final LinkedHashSet<Integer> mTabIds = new LinkedHashSet<>();

    private int mLastShownTabId = Tab.INVALID_TAB_ID;

    TabGroup() {}

    /**
     * Adds a tab to the tab group.
     *
     * @param tabId The ID of the tab to add to the group.
     * @param tabList The list of all tabs in the model containing the tab with tabId. This is used
     *     to sort the tab group to match the order in the tab list.
     */
    void addTab(int tabId, @NonNull TabList tabList) {
        assert tabId != Tab.INVALID_TAB_ID;

        mTabIds.add(tabId);
        if (mLastShownTabId == Tab.INVALID_TAB_ID) setLastShownTabId(tabId);
        if (size() > 1) sortByTabListOrder(tabList);
    }

    /**
     * Removes a tab from the tab group.
     *
     * @param tabId The ID of the tab to remove from the group.
     */
    void removeTab(int tabId) {
        assert mTabIds.contains(tabId);
        if (mLastShownTabId == tabId) {
            int nextIdToShow = nextTabIdToShow(tabId);
            if (nextIdToShow != Tab.INVALID_TAB_ID) setLastShownTabId(nextIdToShow);
        }
        mTabIds.remove(tabId);
    }

    /**
     * Reorders the tab to be at the end of the tab group if this group contains the tab.
     *
     * @param tabId The tab ID to move to the end of the group.
     */
    void moveToEndInGroup(int tabId) {
        if (!mTabIds.contains(tabId)) return;
        mTabIds.remove(tabId);
        mTabIds.add(tabId);
    }

    /** Returns whether the tab group contains a tab with the specified tab ID. */
    boolean contains(int tabId) {
        return mTabIds.contains(tabId);
    }

    /** Returns the size of the tab group. */
    int size() {
        return mTabIds.size();
    }

    /**
     * Returns the list of tab IDs for the tab group in the order the tabs appear in the {@link
     * TabList} they come from.
     */
    List<Integer> getTabIdList() {
        return Collections.unmodifiableList(new ArrayList<>(mTabIds));
    }

    /** Returns the tab ID of the tab within the group that was last selected. */
    int getLastShownTabId() {
        return mLastShownTabId;
    }

    /** Sets the tab ID that was last selected from the group. */
    void setLastShownTabId(int tabId) {
        assert mTabIds.contains(tabId);
        mLastShownTabId = tabId;
    }

    /** Returns the ID of the first tab in the group. */
    int getTabIdOfFirstTab() {
        return mTabIds.stream().findFirst().get();
    }

    /** Returns the ID of the last tab in the group. */
    int getTabIdOfLastTab() {
        return mTabIds.stream().skip(mTabIds.size() - 1).findFirst().get();
    }

    /**
     * Returns the position of a tab in the group.
     *
     * @param tab The tab whose position is to be determined.
     */
    int getPositionOfTab(Tab tab) {
        int index = 0;
        for (int tabId : mTabIds) {
            if (tab.getId() == tabId) {
                return index;
            }
            index++;
        }
        return INVALID_POSITION_IN_GROUP;
    }

    private int nextTabIdToShow(int tabId) {
        if (mTabIds.size() == 1 || !mTabIds.contains(tabId)) return Tab.INVALID_TAB_ID;
        List<Integer> ids = getTabIdList();
        int position = ids.indexOf(tabId);
        if (position == 0) return ids.get(position + 1);
        return ids.get(position - 1);
    }

    private void sortByTabListOrder(@NonNull TabList tabList) {
        for (int i = 0; i < tabList.getCount(); i++) {
            moveToEndInGroup(tabList.getTabAt(i).getId());
        }
    }
}
