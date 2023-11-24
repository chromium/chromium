// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.offline_items_collection.OfflineItem;

/**
 * A {@link OfflineItemFilter} responsible for pruning out off the record items if we are not
 * showing them for this instance of the download manager.
 */
public class OffTheRecordOfflineItemFilter extends OfflineItemFilter {
    private final boolean mIncludeOffTheRecordItems;

    /** Creates an instance of this filter and wraps {@code source}. */
    public OffTheRecordOfflineItemFilter(boolean offTheRecord, OfflineItemFilterSource source) {
        super(source);
        mIncludeOffTheRecordItems = offTheRecord;
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        // Always show downloads from regular mode.
        if (!item.isOffTheRecord) return false;

        // Only show downloads from primary OTR profile if mIncludeOffTheRecordItems is true.
        boolean isPrimaryOTR = OTRProfileID.deserialize(item.otrProfileId).isPrimaryOTRId();
        return !(mIncludeOffTheRecordItems && isPrimaryOTR);
    }
}
