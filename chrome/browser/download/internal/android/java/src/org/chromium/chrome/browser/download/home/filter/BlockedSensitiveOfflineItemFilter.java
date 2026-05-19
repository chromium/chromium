// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

/**
 * A {@link OfflineItemFilter} responsible for pruning out items that are blocked sensitive
 * downloads, when such items should not be shown in the UI for this instance of the download
 * manager.
 */
@NullMarked
public class BlockedSensitiveOfflineItemFilter extends OfflineItemFilter {
    private final boolean mIncludeBlockedSensitiveItems;

    /** Creates an instance of this filter and wraps {@code source}. */
    public BlockedSensitiveOfflineItemFilter(
            DownloadManagerUiConfig config, OfflineItemFilterSource source) {
        super(source);
        mIncludeBlockedSensitiveItems = config.showBlockedSensitiveItems;
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        return !mIncludeBlockedSensitiveItems && DownloadUtils.isBlockedSensitiveDownload(item);
    }
}
