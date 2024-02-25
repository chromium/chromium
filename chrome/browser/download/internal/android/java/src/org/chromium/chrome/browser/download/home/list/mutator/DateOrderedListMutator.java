// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItemModel;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;

/**
 * A class responsible for turning a {@link Collection} of {@link OfflineItem}s into a list meant
 * to be displayed in the download home UI.  This list has the following properties:
 * - Converts changes in the form of {@link Collection}s to delta changes on the list.
 * - Sorting, and adding headers is done by the downstream {@link ListConsumer}s.
 */
public class DateOrderedListMutator implements OfflineItemFilterObserver {
    /**
     * Handles pagination for the list and adds a pagination header at the end, if the list is
     * longer than the desired length. Tracks the number of pages currently being displayed to the
     * user.
     */
    public interface ListPaginator extends ListConsumer {
        /**
         * Increments the currently displayed page count. Called when the pagination header is
         * clicked.
         */
        void loadMorePages();

        /**
         * Resets the pagination tracking. To be called when the filter type of the list is changed.
         */
        void reset();
    }

    private final OfflineItemFilterSource mSource;
    private final JustNowProvider mJustNowProvider;
    private final ListItemModel mModel;
    private ListConsumer mListConsumer;
    private ArrayList<ListItem> mSortedItems = new ArrayList<>();

    /**
     * Creates an DateOrderedList instance that will reflect {@code source}.
     * @param source The source of data for this list.
     * @param model  The model that will be the storage for the updated list.
     * @param justNowProvider The provider for Just Now section.
     */
    public DateOrderedListMutator(
            OfflineItemFilterSource source, ListItemModel model, JustNowProvider justNowProvider) {
        mSource = source;
        mModel = model;
        mJustNowProvider = justNowProvider;
        mSource.addObserver(this);
        onItemsAdded(mSource.getItems());
    }

    /** Sets a {@link ListConsumer} to be notified whenever the list changes. */
    public ListConsumer setListConsumer(ListConsumer consumer) {
        mListConsumer = consumer;
        return mListConsumer;
    }

    /** Called to reload the list and display. */
    public void reload() {
        if (mListConsumer == null) return;
        mListConsumer.onListUpdated(mSortedItems);
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        for (OfflineItem offlineItem : items) {
            OfflineItemListItem listItem = new OfflineItemListItem(offlineItem);
            mSortedItems.add(listItem);
        }

        reload();
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        for (OfflineItem itemToRemove : items) {
            for (int i = 0; i < mSortedItems.size(); i++) {
                OfflineItem offlineItem = ((OfflineItemListItem) mSortedItems.get(i)).item;
                if (itemToRemove.id.equals(offlineItem.id)) mSortedItems.remove(i);
            }
        }

        reload();
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
        assert oldItem.id.equals(item.id);

        // If the update changed the creation time or filter type, remove and add the element to get
        // it positioned.
        if (oldItem.creationTimeMs != item.creationTimeMs
                || oldItem.filter != item.filter
                || mJustNowProvider.isJustNowItem(oldItem)
                        != mJustNowProvider.isJustNowItem(item)) {
            // TODO(shaktisahu): Collect UMA when this happens.
            onItemsRemoved(Collections.singletonList(oldItem));
            onItemsAdded(Collections.singletonList(item));
        } else {
            for (int i = 0; i < mSortedItems.size(); i++) {
                if (item.id.equals(((OfflineItemListItem) mSortedItems.get(i)).item.id)) {
                    mSortedItems.set(i, new OfflineItemListItem(item));
                }
            }
            updateModelListItem(item);
        }

        mModel.dispatchLastEvent();
    }

    private void updateModelListItem(OfflineItem item) {
        for (int i = 0; i < mModel.size(); i++) {
            ListItem listItem = mModel.get(i);
            if (!(listItem instanceof OfflineItemListItem)) continue;

            OfflineItemListItem existingItem = (OfflineItemListItem) listItem;
            if (item.id.equals(existingItem.item.id)) {
                existingItem.item = item;
                mModel.update(i, existingItem);
                break;
            }
        }
    }
}
