// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/**
 * An {@link OfflineItemFilter} responsible for pruning out items that are in the process of being
 * deleted (and can potentially be un-deleted).  This effectively makes a subset of items cease to
 * exist for all down-stream consumers.
 */
public class DeleteUndoOfflineItemFilter extends OfflineItemFilter {
    private final Set<ContentId> mPendingDeletions = new HashSet<>();

    /** Creates an instance of this filter and wraps {@code source}. */
    public DeleteUndoOfflineItemFilter(OfflineItemFilterSource source) {
        super(source);
        onFilterChanged();
    }

    /** Adds {@code items} to the set of items that should currently appear to be gone. */
    public void addPendingDeletions(Collection<OfflineItem> items) {
        for (OfflineItem item : items) mPendingDeletions.add(item.id);
        onFilterChanged();
    }

    /** Removes {@code items} from the set of items that should currently appear to be gone. */
    public void removePendingDeletions(Collection<OfflineItem> items) {
        for (OfflineItem item : items) mPendingDeletions.remove(item.id);
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        super.onItemsRemoved(items);
        for (OfflineItem item : items) mPendingDeletions.remove(item.id);
    }

    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        if (mPendingDeletions == null) return false;
        return mPendingDeletions.contains(item.id);
    }
}
