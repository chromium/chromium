// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collections;
import java.util.List;

/**
 * Sorter based on download date. Items having same date (day of download creation), will be
 * compared based on mime type. For further tie-breakers, timestamp and ID are used.
 * Note, the input list must contain only offline items.
 */
public class DateSorter implements ListConsumer {
    private final JustNowProvider mJustNowProvider;
    private ListConsumer mListConsumer;

    public DateSorter(@Nullable JustNowProvider justNowProvider) {
        mJustNowProvider = justNowProvider;
    }

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

    private List<ListItem> sort(List<ListItem> list) {
        Collections.sort(list, this::compare);
        return list;
    }

    private int compare(ListItem listItem1, ListItem listItem2) {
        OfflineItem lhs = ((ListItem.OfflineItemListItem) listItem1).item;
        OfflineItem rhs = ((ListItem.OfflineItemListItem) listItem2).item;

        int comparison = compareItemByJustNowProvider(lhs, rhs);
        if (comparison != 0) return comparison;

        comparison = ListUtils.compareItemByDate(lhs, rhs);
        if (comparison != 0) return comparison;

        comparison =
                ListUtils.compareFilterTypesTo(
                        Filters.fromOfflineItem(lhs), Filters.fromOfflineItem(rhs));
        if (comparison != 0) return comparison;

        comparison = ListUtils.compareItemByTimestamp(lhs, rhs);
        if (comparison != 0) return comparison;

        return ListUtils.compareItemByID(lhs, rhs);
    }

    private int compareItemByJustNowProvider(OfflineItem lhs, OfflineItem rhs) {
        boolean lhsIsJustNowItem = mJustNowProvider != null && mJustNowProvider.isJustNowItem(lhs);
        boolean rhsIsJustNowItem = mJustNowProvider != null && mJustNowProvider.isJustNowItem(rhs);
        if (lhsIsJustNowItem == rhsIsJustNowItem) return 0;
        return lhsIsJustNowItem && !rhsIsJustNowItem ? -1 : 1;
    }
}
