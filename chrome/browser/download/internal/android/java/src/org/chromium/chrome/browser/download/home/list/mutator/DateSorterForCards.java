// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Sorter based on download date that also takes into account the items grouped into a card. For
 * items grouped in a card, the timestamp of the most recent item will be used for comparison
 * purposes. Note, the input list must contain only offline items.
 */
public class DateSorterForCards implements ListConsumer {
    private ListConsumer mListConsumer;
    private Map<String, Long> mTimestampForCard = new HashMap<>();

    @Override
    public ListConsumer setListConsumer(ListConsumer consumer) {
        mListConsumer = consumer;
        return mListConsumer;
    }

    @Override
    public void onListUpdated(List<ListItem> inputList) {
        if (mListConsumer == null) return;
        mListConsumer.onListUpdated(sort(inputList));
    }

    private List<ListItem> sort(List<ListItem> inputList) {
        setTimestampForCards(inputList);
        Collections.sort(inputList, this::compare);
        return inputList;
    }

    private int compare(ListItem listItem1, ListItem listItem2) {
        OfflineItem lhs = ((ListItem.OfflineItemListItem) listItem1).item;
        OfflineItem rhs = ((ListItem.OfflineItemListItem) listItem2).item;

        // Compare items by timestamp. For group items, use most recent timestamp.
        int comparison =
                Long.compare(getTimestampForItem(listItem2), getTimestampForItem(listItem1));
        if (comparison != 0) return comparison;

        // We are probably comparing two items of the same card. Show the most recent one first.
        comparison = ListUtils.compareItemByTimestamp(lhs, rhs);
        if (comparison != 0) return comparison;

        return ListUtils.compareItemByID(lhs, rhs);
    }

    private void setTimestampForCards(List<ListItem> inputList) {
        mTimestampForCard.clear();

        // For items having same domain, use the timestamp of the most recent item for comparison.
        for (ListItem listItem : inputList) {
            if (!ListUtils.canGroup(listItem)) continue;

            OfflineItem offlineItem = ((ListItem.OfflineItemListItem) listItem).item;
            String domain = UiUtils.getDomainForItem(offlineItem);
            long timestampForCard =
                    mTimestampForCard.containsKey(domain) ? mTimestampForCard.get(domain) : 0;
            mTimestampForCard.put(domain, Math.max(timestampForCard, offlineItem.creationTimeMs));
        }
    }

    private long getTimestampForItem(ListItem listItem) {
        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) listItem).item;
        if (ListUtils.canGroup(listItem)) {
            String domain = UiUtils.getDomainForItem(offlineItem);
            return mTimestampForCard.get(domain);
        }
        return offlineItem.creationTimeMs;
    }
}
