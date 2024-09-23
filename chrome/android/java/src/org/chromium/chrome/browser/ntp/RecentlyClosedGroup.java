// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;

/** Represents a recently closed group from TabRestoreService. */
public class RecentlyClosedGroup extends RecentlyClosedEntry {
    private final String mTitle;
    private final @TabGroupColorId int mColor;
    private final List<RecentlyClosedTab> mTabs = new ArrayList<>();

    public RecentlyClosedGroup(
            int sessionId, long timestamp, String title, @TabGroupColorId int color) {
        super(sessionId, timestamp);
        mTitle = title;
        mColor = color;
    }

    /**
     * Returns the title of the group this may be an empty string if the default title was used when
     * saving.
     */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the color of the group. */
    public @TabGroupColorId int getColor() {
        return mColor;
    }

    /** Returns the list of tabs for this group. */
    public List<RecentlyClosedTab> getTabs() {
        return mTabs;
    }
}
