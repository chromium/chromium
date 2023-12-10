// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import java.util.ArrayList;
import java.util.List;

/** Represents a recently closed group from TabRestoreService. */
public class RecentlyClosedGroup extends RecentlyClosedEntry {
    private final String mTitle;
    private final List<RecentlyClosedTab> mTabs = new ArrayList<>();

    public RecentlyClosedGroup(int sessionId, long timestamp, String title) {
        super(sessionId, timestamp);
        mTitle = title;
    }

    /**
     * @return title of the group this may be an empty string if the default title was used when
     * saving.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return the list of tabs for this group.
     */
    public List<RecentlyClosedTab> getTabs() {
        return mTabs;
    }
}
