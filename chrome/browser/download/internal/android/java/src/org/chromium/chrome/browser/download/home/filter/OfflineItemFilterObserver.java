// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/**
 * An observer that will be notified on changes to an underlying
 * {@link OfflineItemFilterSource}.
 */
public interface OfflineItemFilterObserver {
    /**
     * Called when items have been added to the observed {@link OfflineItemFilterSource}.
     * @param items A collection of {@link OfflineItem}s that have been added.
     */
    default void onItemsAdded(Collection<OfflineItem> items) {}

    /**
     * Called when items have been removed from the observed {@link OfflineItemFilterSource}.
     * @param items A collection of {@link OfflineItem}s that have been removed.
     */
    default void onItemsRemoved(Collection<OfflineItem> items) {}

    /**
     * Called when an {@link OfflineItem} has been updated.  The old and new items are sent for easy
     * set lookup and replacement.
     * @param oldItem The old {@link OfflineItem} before the update.
     * @param item    The new {@link OfflineItem} after the update.
     */
    default void onItemUpdated(OfflineItem oldItem, OfflineItem item) {}

    /**
     * Called when the underlying {@link OfflineItem}s are available.  This is meant to help detect
     * a difference between an empty set and a set that is not loaded yet.
     */
    default void onItemsAvailable() {}
}
