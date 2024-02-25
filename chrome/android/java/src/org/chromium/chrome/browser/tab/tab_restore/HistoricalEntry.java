// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import org.chromium.chrome.browser.tab.Tab;

import java.util.Collections;
import java.util.List;

/** Base class representing a historical entry. Can be used for either a group or single tab. */
public class HistoricalEntry {
    private final int mRootId;
    private final String mGroupTitle;
    private final List<Tab> mTabs;

    /**
     * @param tab The tab for this entry.
     */
    public HistoricalEntry(Tab tab) {
        mRootId = Tab.INVALID_TAB_ID;
        mGroupTitle = null;
        mTabs = Collections.singletonList(tab);
    }

    /**
     * @param rootId The root ID of this entry. This is only used for grouping purposes and is not
     *     saved.
     * @param groupTitle The title of the group or null if the default group name should be used.
     * @param tabs The list of {@link Tab} in this group.
     */
    public HistoricalEntry(int rootId, String groupTitle, List<Tab> tabs) {
        assert rootId != Tab.INVALID_TAB_ID;
        mRootId = rootId;
        mGroupTitle = groupTitle;
        // Allow single tab groups here. These can be collapsed to tab form during validation.
        mTabs = tabs;
    }

    /** Returns whether this entry is a single tab. */
    public boolean isSingleTab() {
        return mTabs.size() == 1 && mRootId == Tab.INVALID_TAB_ID;
    }

    /** Returns the root ID for the group this entry represents. */
    public int getRootId() {
        return mRootId;
    }

    /** Returns the title of the group this entry represents. */
    public String getGroupTitle() {
        return mGroupTitle;
    }

    /** Returns the list of tabs in this group. */
    public List<Tab> getTabs() {
        return mTabs;
    }
}
