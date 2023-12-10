// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/**
 * A filtered source of {@link OfflineItem}s.  This class supports querying a collection of
 * {@link OfflineItem}s as well as registration of observers to be notified of changes.
 */
public interface OfflineItemFilterSource {
    /**
     * @return The current collection of {@link OfflineItem}s as understood by this source.  Note
     * that this list may be filtered by the source itself if it deems it necessary.
     */
    Collection<OfflineItem> getItems();

    /**
     * @return Whether or not the items are available, which is meant to help determine the
     * difference between an empty set and a set that hasn't loaded yet.
     */
    boolean areItemsAvailable();

    /**
     * Registers {@code observer} to be notified of changes to the item collection managed by this
     * source.
     */
    void addObserver(OfflineItemFilterObserver observer);

    /**
     * Unregisters {@code observer} from notifications of changes to the item collection managed by
     * this source.
     */
    void removeObserver(OfflineItemFilterObserver observer);
}
