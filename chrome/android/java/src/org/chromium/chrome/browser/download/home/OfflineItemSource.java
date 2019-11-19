// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.glue.OfflineContentProviderGlue;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * The source of {@link OfflineItem} for the rest of the download home UI.  This will pull items
 * from a {@link OfflineContentProvider} as well as the downloads backend and unify them to a single
 * list for the rest of the UI to filter and act on.
 */
public class OfflineItemSource implements OfflineItemFilterSource, OfflineContentProvider.Observer {
    // TODO(dtrainor): Move this to OfflineContentProvider once downloads are ported.
    private final OfflineContentProviderGlue mProvider;

    private final Map<ContentId, OfflineItem> mItems = new HashMap<>();
    private final ObserverList<OfflineItemFilterObserver> mObservers = new ObserverList<>();

    /** Used to track whether or not the items have been loaded from {@code mProvider} or not. */
    private boolean mItemsAvailable;

    /**
     * Used to track whether or not this is destroyed so we know whether or not to do additional
     * work when outstanding callbacks return.
     */
    private boolean mDestroyed;

    /**
     * Creates an instance of {@link OfflineItemSource} and hooks up to {@code provider}.  This will
     * automatically try to pull all existing items from {@code provider}.
     * @param provider The {@link OfflineContentProviderGlue} to reflect in this source.
     */
    public OfflineItemSource(OfflineContentProviderGlue provider) {
        mProvider = provider;
        mProvider.addObserver(this);

        mProvider.getAllItems(items -> {
            if (mDestroyed) return;

            mItemsAvailable = true;
            for (OfflineItemFilterObserver observer : mObservers) observer.onItemsAvailable();
            onItemsAdded(items);
        });
    }

    /**
     * Destroys this object which means it (1) no longer contains any items and (2) no longer
     * notifies any observers of changes.
     */
    public void destroy() {
        mProvider.removeObserver(this);
        mObservers.clear();
        mItems.clear();
        mDestroyed = true;
    }

    // OfflineItemFilterSource implementation.
    @Override
    public Collection<OfflineItem> getItems() {
        return mItems.values();
    }

    @Override
    public boolean areItemsAvailable() {
        return mItemsAvailable;
    }

    @Override
    public void addObserver(OfflineItemFilterObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(OfflineItemFilterObserver observer) {
        mObservers.removeObserver(observer);
    }

    // OfflineContentProvider.Observer implementation.
    @Override
    public void onItemsAdded(ArrayList<OfflineItem> items) {
        Set<OfflineItem> addedItems = new HashSet<OfflineItem>();
        for (OfflineItem item : items) {
            if (mItems.containsKey(item.id)) {
                onItemUpdated(item, null);
            } else {
                mItems.put(item.id, item);
                addedItems.add(item);
            }
        }

        if (addedItems.size() > 0) {
            for (OfflineItemFilterObserver observer : mObservers) observer.onItemsAdded(addedItems);
        }
    }

    @Override
    public void onItemRemoved(ContentId id) {
        OfflineItem item = mItems.remove(id);
        if (item == null) return;

        Set<OfflineItem> removedSet = CollectionUtil.newHashSet(item);
        for (OfflineItemFilterObserver observer : mObservers) observer.onItemsRemoved(removedSet);
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
        OfflineItem oldItem = mItems.get(item.id);
        if (oldItem == null) {
            onItemsAdded(CollectionUtil.newArrayList(item));
        } else {
            mItems.put(item.id, item);
            for (OfflineItemFilterObserver observer : mObservers) {
                observer.onItemUpdated(oldItem, item);
            }
        }
    }
}
