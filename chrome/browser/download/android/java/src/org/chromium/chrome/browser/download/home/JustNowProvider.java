// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.Date;
import java.util.HashSet;
import java.util.Set;

/** Helper class to expose whether an item should be shown in the Just Now section. */
public class JustNowProvider {
    // Threshold timestamp after which a download is considered recent.
    private final Date mThresholdDate;

    // Tracks the items that are being shown in the Just Now section. Note, these items are only
    // added, and never removed. This is because this class will go away after closing download
    // home.
    private final Set<ContentId> mItems = new HashSet<>();

    /** Constructor. */
    public JustNowProvider(DownloadManagerUiConfig config) {
        mThresholdDate = new Date(now().getTime() - config.justNowThresholdSeconds * 1000);
    }

    /**
     * @return Whether the given {@code item} should be shown in the Just Now section.
     */
    public boolean isJustNowItem(OfflineItem item) {
        boolean shouldBeJustNowItem = isRecentOrInProgressDownload(item);
        if (shouldBeJustNowItem) mItems.add(item.id);
        return mItems.contains(item.id);
    }

    private boolean isRecentOrInProgressDownload(OfflineItem item) {
        return item.state == OfflineItemState.IN_PROGRESS
                || item.state == OfflineItemState.PAUSED
                || (item.state == OfflineItemState.INTERRUPTED && item.isResumable)
                || new Date(item.completionTimeMs).after(mThresholdDate);
    }

    @VisibleForTesting
    protected Date now() {
        return new Date();
    }
}
