// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ObserverList;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

/**
 * An abstract class responsible for consuming a collection of {@link OfflineItem}s, filtering it
 * based on subclass criteria, and exposing the filtered collection to observers.  This class is
 * meant to make it easy to chain various filter stages together while keeping each particular
 * filter self-contained and testable.
 *
 * It is recommended that subclasses call {@link #onFilterChanged()} after initialization to
 * populate this class with the correctly filtered set of objects.  This constructor cannot call
 * that method as {@link #isFilteredOut(OfflineItem)} might access unconstructed subclass state.
 */
public abstract class OfflineItemFilter
        implements OfflineItemFilterObserver, OfflineItemFilterSource {
    private final OfflineItemFilterSource mSource;

    private final Set<OfflineItem> mItems = new HashSet<>();
    private final ObserverList<OfflineItemFilterObserver> mObservers = new ObserverList<>();

    /**
     * Creates a filter wrapping a filter source {@code source}.  After creation this class should
     * represent the filtered state of {code source}.  This class will also update based on any
     * changes to {@code source} automatically.  Note that this class will *not* unregister as an
     * observer on {@code source}.
     */
    public OfflineItemFilter(OfflineItemFilterSource source) {
        mSource = source;
        source.addObserver(this);
    }

    /**
     * Will be called based on changes to the underlying {@link OfflineItemFilterSource}.  This will
     * determine which elements get filtered out of the exposed collection of {@link OfflineItem}s.
     * @param item The {@link OfflineItem} to check.
     * @return     Whether or not the {@link OfflineItem} should be filtered out by this class.
     */
    protected abstract boolean isFilteredOut(OfflineItem item);

    /**
     * Called by subclasses to notify this class that the filtering criteria have changed.  When
     * this happens this class will reevaluate the current filter state and send appropriate
     * {@link OfflineItemFilterObserver#onItemsAdded(Collection)} and
     * {@link OfflineItemFilterObserver#onItemsRemoved(Collection)} calls to observers downstream.
     */
    protected void onFilterChanged() {
        Set<OfflineItem> removed = new HashSet<>();
        for (Iterator<OfflineItem> iter = mItems.iterator(); iter.hasNext(); ) {
            OfflineItem item = iter.next();
            if (isFilteredOut(item)) {
                iter.remove();
                removed.add(item);
            }
        }
        if (!removed.isEmpty()) {
            for (OfflineItemFilterObserver obs : mObservers) obs.onItemsRemoved(removed);
        }

        addItems(mSource.getItems());
    }

    // OfflineItemFilterSource implementation.
    @Override
    public Set<OfflineItem> getItems() {
        return mItems;
    }

    @Override
    public boolean areItemsAvailable() {
        return mSource.areItemsAvailable();
    }

    @Override
    public void addObserver(OfflineItemFilterObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(OfflineItemFilterObserver observer) {
        mObservers.removeObserver(observer);
    }

    // OfflineItemSetObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        addItems(items);
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        Set<OfflineItem> removed = new HashSet<>();
        for (OfflineItem item : items) {
            if (mItems.remove(item)) removed.add(item);
        }

        if (!removed.isEmpty()) {
            for (OfflineItemFilterObserver obs : mObservers) obs.onItemsRemoved(removed);
        }
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
        boolean oldInList = mItems.remove(oldItem);
        boolean newInList = !isFilteredOut(item);

        if (oldInList && newInList) {
            mItems.add(item);
            for (OfflineItemFilterObserver obs : mObservers) obs.onItemUpdated(oldItem, item);
        } else if (!oldInList && newInList) {
            mItems.add(item);
            Collection<OfflineItem> newItems = CollectionUtil.newHashSet(item);
            for (OfflineItemFilterObserver obs : mObservers) obs.onItemsAdded(newItems);
        } else if (oldInList && !newInList) {
            Collection<OfflineItem> oldItems = CollectionUtil.newHashSet(oldItem);
            for (OfflineItemFilterObserver obs : mObservers) obs.onItemsRemoved(oldItems);
        }
    }

    @Override
    public void onItemsAvailable() {
        for (OfflineItemFilterObserver obs : mObservers) obs.onItemsAvailable();
    }

    // Helper method to help incorporate a collection of items into this filtered version.
    private void addItems(Collection<OfflineItem> items) {
        Set<OfflineItem> added = new HashSet<>();
        for (OfflineItem item : items) {
            if (isFilteredOut(item)) continue;
            if (mItems.add(item)) added.add(item);
        }

        if (!added.isEmpty()) {
            for (OfflineItemFilterObserver obs : mObservers) obs.onItemsAdded(added);
        }
    }
}
