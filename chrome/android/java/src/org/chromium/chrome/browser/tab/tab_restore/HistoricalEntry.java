// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import org.chromium.chrome.browser.tab.Tab;

import java.util.Collections;
import java.util.List;

/** Base class representing a historical entry. Can be used for either a group or single tab. */
public class HistoricalEntry {
    private final int mGroupId;
    private final String mGroupTitle;
    private final List<Tab> mTabs;

    /**
     * @param tab The tab for this entry.
     */
    public HistoricalEntry(Tab tab) {
        mGroupId = Tab.INVALID_TAB_ID;
        mGroupTitle = null;
        mTabs = Collections.singletonList(tab);
    }

    /**
     * @param groupId The group ID of this entry. This is only used for grouping purposes and is not
     *           saved.
     * @param groupTitle The title of the group or null if the default group name should be used.
     * @param tabs The list of {@link Tab} in this group.
     */
    public HistoricalEntry(int groupId, String groupTitle, List<Tab> tabs) {
        assert groupId != Tab.INVALID_TAB_ID;
        mGroupId = groupId;
        mGroupTitle = groupTitle;
        // Allow single tab groups here. These can be collapsed to tab form during validation.
        mTabs = tabs;
    }

    /**
     * @return Whether this entry is a single tab.
     */
    public boolean isSingleTab() {
        return mTabs.size() == 1 && mGroupId == Tab.INVALID_TAB_ID;
    }

    /**
     * @return The Android Group Id for the group this entry represents.
     */
    public int getGroupId() {
        return mGroupId;
    }

    /**
     * @return The title of the group this entry represents.
     */
    public String getGroupTitle() {
        return mGroupTitle;
    }

    /**
     * @return The list of tabs in this group.
     */
    public List<Tab> getTabs() {
        return mTabs;
    }
}
