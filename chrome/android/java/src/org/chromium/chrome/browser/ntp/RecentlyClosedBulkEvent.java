// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Represents a recent closure of multiple tabs and groups (AKA Window) from TabRestoreService.
 */
public class RecentlyClosedBulkEvent extends RecentlyClosedEntry {
    private final List<RecentlyClosedTab> mTabs = new ArrayList<>();
    private final Map<String, String> mGroups = new HashMap<>();

    public RecentlyClosedBulkEvent(int sessionId, long timestamp) {
        super(sessionId, timestamp);
    }

    /**
     * @return list of {@link RecentlyClosedTab} in this event.
     */
    public List<RecentlyClosedTab> getTabs() {
        return mTabs;
    }

    /**
     * @return map of {@link RecentlyClosedTab#getGroupId()} to group titles.
     */
    public Map<String, String> getGroupIdToTitleMap() {
        return mGroups;
    }
}
