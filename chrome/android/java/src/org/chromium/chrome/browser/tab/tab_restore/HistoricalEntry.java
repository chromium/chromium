// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Collections;
import java.util.List;

/** Base class representing a historical entry. Can be used for either a group or single tab. */
@NullMarked
public class HistoricalEntry {
    private final @Nullable Token mTabGroupId;
    private final @Nullable String mGroupTitle;
    private final @TabGroupColorId int mGroupColor;
    private final List<Tab> mTabs;

    /**
     * Constructor for a tab that is closing.
     *
     * <p>Closing tabs that are a proper subset of a tab group should use this constructor. If a tab
     * group with only a single tab is closing use the tab group constructor.
     *
     * @param tab The tab for this entry.
     */
    public HistoricalEntry(Tab tab) {
        // TODO(crbug/327166316): individual tabs are not treated as part of a tab group on Android.
        // This should be changed to align with desktop, but this requires plumbing this information
        // through the native TabAndroid object rather than here. For now this is just used for
        // managing grouping.
        mTabGroupId = null;
        mGroupTitle = null;
        // Apply the first color in the color list as the default, since a single tab item should
        // not have a color associated with it anyways.
        mGroupColor = TabGroupColorId.GREY;
        mTabs = Collections.singletonList(tab);
    }

    /**
     * Constructor for a tab group that is closing.
     *
     * @param tabGroupId The tab group id of the group.
     * @param groupTitle The title of the group or null if the default group name should be used.
     * @param groupColor The {@TabGroupColorId} of the group.
     * @param tabs The list of {@link Tab} in this group.
     */
    public HistoricalEntry(
            Token tabGroupId,
            @Nullable String groupTitle,
            @TabGroupColorId int groupColor,
            List<Tab> tabs) {
        assert tabGroupId != null;
        mTabGroupId = tabGroupId;
        mGroupTitle = groupTitle;
        mGroupColor = groupColor;
        mTabs = tabs;
    }

    /** Returns whether this entry is a single tab. */
    public boolean isSingleTab() {
        return mTabs.size() == 1 && mTabGroupId == null;
    }

    /** Returns the tab group ID for the group this entry represents; null for single tabs. */
    public @Nullable Token getTabGroupId() {
        return mTabGroupId;
    }

    /**
     * Returns the title of the group this entry represents; null for single tabs or default title.
     */
    public @Nullable String getGroupTitle() {
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
