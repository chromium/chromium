// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Collections;
import java.util.List;

/** Base class representing a historical entry. Can be used for either a group or single tab. */
public class HistoricalEntry {
    private final int mRootId;
    private final @Nullable Token mTabGroupId;
    private final @Nullable String mGroupTitle;
    private final @TabGroupColorId int mGroupColor;
    private final List<Tab> mTabs;

    /**
     * Constructor for individual tabs that aren't a tab group of size 1. Individual tabs that are
     * part of a group with more that one tab should use this constructor.
     *
     * @param tab The tab for this entry.
     */
    public HistoricalEntry(Tab tab) {
        mRootId = Tab.INVALID_TAB_ID;
        // TODO(crbug/327166316): individual tabs are not treated as part of a tab group on Android.
        // This should be changed to align with desktop, but this required plumbing this information
        // through the native TabAndroid object rather than here. For now assume this is unused.
        mTabGroupId = null;
        mGroupTitle = null;
        // Apply the first color in the color list as the default, since a single tab item should
        // not have a color associated with it anyways.
        mGroupColor = TabGroupColorId.GREY;
        mTabs = Collections.singletonList(tab);
    }

    /**
     * Constructor for an entire tab group.
     *
     * @param rootId The root ID of this entry. This is only used for grouping purposes and is not
     *     saved.
     * @param tabGroupId The tab group id of the group. This is saved if it is used.
     * @param groupTitle The title of the group or null if the default group name should be used.
     * @param groupColor The {@TabGroupColorId} of the group.
     * @param tabs The list of {@link Tab} in this group.
     */
    public HistoricalEntry(
            int rootId,
            @Nullable Token tabGroupId,
            @Nullable String groupTitle,
            @TabGroupColorId int groupColor,
            List<Tab> tabs) {
        assert rootId != Tab.INVALID_TAB_ID;
        mRootId = rootId;
        mTabGroupId = tabGroupId;
        mGroupTitle = groupTitle;
        mGroupColor = groupColor;
        mTabs = tabs;
    }

    /** Returns whether this entry is a single tab. */
    public boolean isSingleTab() {
        return mTabs.size() == 1 && mTabGroupId == null;
    }

    /** Returns the root ID for the group this entry represents. */
    public int getRootId() {
        return mRootId;
    }

    /** Returns the tab group ID for the group this entry represents. */
    public @Nullable Token getTabGroupId() {
        return mTabGroupId;
    }

    /** Returns the title of the group this entry represents. */
    public String getGroupTitle() {
        return mGroupTitle;
    }

    /** Returns the color of the group this entry represents. */
    public @TabGroupColorId int getGroupColor() {
        return mGroupColor;
    }

    /** Returns the list of tabs in this group. */
    public List<Tab> getTabs() {
        return mTabs;
    }
}
