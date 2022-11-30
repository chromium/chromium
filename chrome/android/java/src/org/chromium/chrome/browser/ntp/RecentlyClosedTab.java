// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.url.GURL;

/**
 * Represents a recently closed tab from TabRestoreService.
 */
public class RecentlyClosedTab extends RecentlyClosedEntry {
    private final String mTitle;
    private final GURL mUrl;
    /**
     * Native tab_groups::TabGroupId. This is NOT equal to {@link RecentlyClosedEntry#id} for the
     * corresponding {@link RecentlyClosedGroup}.
     */
    private final String mGroupId;

    public RecentlyClosedTab(
            int sessionId, long timestamp, String title, GURL url, String groupId) {
        super(sessionId, timestamp);
        mTitle = title;
        mUrl = url;

        // Treat null and empty strings as equivalent.
        mGroupId = (groupId == null || groupId.isEmpty()) ? null : groupId;
    }

    /**
     * @return Title of tab.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return URL of the tab.
     */
    public GURL getUrl() {
        return mUrl;
    }

    /**
     * @return Group ID of the tab. Useful when displaying groups from a {@link
     *         RecentlyClosedBulkEvent}.
     */
    public String getGroupId() {
        return mGroupId;
    }
}
