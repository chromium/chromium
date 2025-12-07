// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.components.offline_items_collection.OfflineItem;

/**
 * A {@link OfflineItemFilter} responsible for pruning out items that are dangerous, when such items
 * should not be shown in the UI for this instance of the download manager.
 */
@NullMarked
public class DangerousOfflineItemFilter extends OfflineItemFilter {
    private final boolean mIncludeDangerousItems;

    /** Creates an instance of this filter and wraps {@code source}. */
    public DangerousOfflineItemFilter(
            DownloadManagerUiConfig config, OfflineItemFilterSource source) {
        super(source);
        mIncludeDangerousItems = config.showDangerousItems;
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        return !mIncludeDangerousItems && item.isDangerous;
    }
}
