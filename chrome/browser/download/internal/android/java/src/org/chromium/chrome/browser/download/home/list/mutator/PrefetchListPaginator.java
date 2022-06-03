// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles pagination for the prefetch tab. Always ensures that the items in a card are displayed
 * fully even if the total item count might exceed the desired limit.
 */
public class PrefetchListPaginator implements DateOrderedListMutator.ListPaginator {
    private static final int DEFAULT_PAGE_SIZE = 25;

    private ListConsumer mListConsumer;
    private int mCurrentPageIndex;

    @Override
    public ListConsumer setListConsumer(ListConsumer consumer) {
        mListConsumer = consumer;
        return mListConsumer;
    }

    @Override
    public void onListUpdated(List<ListItem> inputList) {
        if (mListConsumer == null) return;
        mListConsumer.onListUpdated(getPaginatedList(inputList));
    }

    @Override
    public void loadMorePages() {
        mCurrentPageIndex++;
    }

    @Override
    public void reset() {
        mCurrentPageIndex = 0;
    }

    private List<ListItem> getPaginatedList(List<ListItem> inputList) {
        List<ListItem> outputList = new ArrayList<>();

        boolean showPagination = false;
        boolean seenCardHeader = false;
        for (ListItem item : inputList) {
            boolean reachedMax = outputList.size() >= (mCurrentPageIndex + 1) * DEFAULT_PAGE_SIZE;

            if (reachedMax) {
                if (!seenCardHeader) {
                    showPagination = true;
                    break;
                }
            }

            seenCardHeader |= item instanceof ListItem.CardHeaderListItem;
            if (isCardFooter(item)) {
                seenCardHeader = false;
            }

            outputList.add(item);
        }

        if (showPagination) outputList.add(new ListItem.PaginationListItem());

        return outputList;
    }

    private boolean isCardFooter(ListItem listItem) {
        if (!(listItem instanceof ListItem.CardDividerListItem)) return false;
        ListItem.CardDividerListItem item = (ListItem.CardDividerListItem) listItem;
        return item.position == ListItem.CardDividerListItem.Position.BOTTOM;
    }
}
