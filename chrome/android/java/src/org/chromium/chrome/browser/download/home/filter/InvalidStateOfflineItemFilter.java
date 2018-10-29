// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

/** A {@link OfflineItemFilter} responsible for pruning out items that do not have the right state
 *  to show in the UI or have been externally deleted.
 */
public class InvalidStateOfflineItemFilter extends OfflineItemFilter {
    /** Creates an instance of this filter and wraps {@code source}. */
    public InvalidStateOfflineItemFilter(OfflineItemFilterSource source) {
        super(source);
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        if (item.externallyRemoved) return true;

        switch (item.state) {
            case OfflineItemState.CANCELLED:
            case OfflineItemState.FAILED:
                return true;
            case OfflineItemState.INTERRUPTED:
                return !item.isResumable;
            default:
                return false;
        }
    }
}