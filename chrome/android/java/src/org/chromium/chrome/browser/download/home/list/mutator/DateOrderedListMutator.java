// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CollectionUtil;
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
import java.util.Comparator;
import java.util.List;

/**
 * A class responsible for turning a {@link Collection} of {@link OfflineItem}s into a list meant
 * to be displayed in the download home UI.  This list has the following properties:
 * - Sorted, according to a given {@link Comparator}.
 * - Separated by labels, such as date header, pagination header or domain header.
 * - Converts changes in the form of {@link Collection}s to delta changes on the list.
 */
public class DateOrderedListMutator implements OfflineItemFilterObserver {
    /**
     * Given a sorted list of {@link OfflineItem}, generates a list of {@link ListItem} with
     * appropriate labels inserted at the right positions as per the display requirements.
     */
    public interface LabelAdder {
        /**
         * Inserts various labels to the given list as per display requirements.
         * @param sortedList The input list to be displayed.
         * @return The output list to be displayed on screen.
         */
        List<ListItem> addLabels(List<OfflineItem> sortedList);
    }

    private final OfflineItemFilterSource mSource;
    private final JustNowProvider mJustNowProvider;
    private final ListItemModel mModel;
    private final Paginator mPaginator;
    private final ArrayList<OfflineItem> mSortedItems = new ArrayList<>();

    private Comparator<OfflineItem> mComparator;
    private LabelAdder mLabelAdder;

    /**
     * Creates an DateOrderedList instance that will reflect {@code source}.
     * @param source The source of data for this list.
     * @param model  The model that will be the storage for the updated list.
     * @param justNowProvider The provider for Just Now section.
     * @param comparator The default comparator to be used for list item comparison
     * @param labelAdder The label adder used for producing the final list.
     * @param paginator The paginator to handle pagination.
     */
    public DateOrderedListMutator(OfflineItemFilterSource source, ListItemModel model,
            JustNowProvider justNowProvider, Comparator<OfflineItem> comparator,
            LabelAdder labelAdder, Paginator paginator) {
        mSource = source;
        mModel = model;
        mJustNowProvider = justNowProvider;
        mPaginator = paginator;
        mSource.addObserver(this);
        setMutators(comparator, labelAdder);
        onItemsAdded(mSource.getItems());
    }

    /**
     * Called when the desired sorting order has changed.
     * @param comparator The comparator to use for list item comparison.
     * @param labelAdder The label adder used for producing the final list.
     */
    public void setMutators(Comparator<OfflineItem> comparator, LabelAdder labelAdder) {
        if (mComparator == comparator && mLabelAdder == labelAdder) return;
        mComparator = comparator;
        mLabelAdder = labelAdder;
        Collections.sort(mSortedItems, mComparator);
    }

    /**
     * Called to add more pages to show in the list.
     */
    public void loadMorePages() {
        mPaginator.loadMorePages();
        pushItemsToModel();
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        ArrayList<OfflineItem> itemsToAdd = new ArrayList<>(items);
        Collections.sort(itemsToAdd, mComparator);
        mergeList(mSortedItems, itemsToAdd, mComparator);
        pushItemsToModel();
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        for (OfflineItem itemToRemove : items) {
            for (int i = 0; i < mSortedItems.size(); i++) {
                if (itemToRemove.id.equals(mSortedItems.get(i).id)) mSortedItems.remove(i);
            }
        }
        pushItemsToModel();
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
        assert oldItem.id.equals(item.id);

        // If the update changed the creation time or filter type, remove and add the element to get
        // it positioned.
        if (oldItem.creationTimeMs != item.creationTimeMs || oldItem.filter != item.filter
                || mJustNowProvider.isJustNowItem(oldItem)
                        != mJustNowProvider.isJustNowItem((item))) {
            // TODO(shaktisahu): Collect UMA when this happens.
            onItemsRemoved(CollectionUtil.newArrayList(oldItem));
            onItemsAdded(CollectionUtil.newArrayList(item));
        } else {
            for (int i = 0; i < mSortedItems.size(); i++) {
                if (item.id.equals(mSortedItems.get(i).id)) mSortedItems.set(i, item);
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

    private void pushItemsToModel() {
        // TODO(shaktisahu): Add paginated list after finalizing UX.
        mModel.set(mLabelAdder.addLabels(mSortedItems));
        mModel.dispatchLastEvent();
    }

    /**
     * Merges two sorted lists list1 and list2 and places the result in list1.
     * @param list1 The first list, which is also the output.
     * @param list2 The second list.
     * @param comparator The comparison function to use.
     */
    @VisibleForTesting
    void mergeList(
            List<OfflineItem> list1, List<OfflineItem> list2, Comparator<OfflineItem> comparator) {
        int index1 = 0;
        int index2 = 0;

        while (index2 < list2.size()) {
            OfflineItem itemToAdd = list2.get(index2);
            boolean foundInsertionPoint =
                    index1 == list1.size() || comparator.compare(itemToAdd, list1.get(index1)) < 0;
            if (foundInsertionPoint) {
                list1.add(index1, itemToAdd);
                index2++;
            }
            index1++;
        }
    }
}
