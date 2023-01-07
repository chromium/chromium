// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles pagination for a given date ordered list. Always ensures that all the items for a given
 * date are displayed fully even if the total item count might exceed the desired limit.
 */
public class DateListPaginator implements DateOrderedListMutator.ListPaginator {
    private static final int DEFAULT_PAGE_SIZE = 25;

    private ListConsumer mListConsumer;
    private int mCurrentPageIndex;

    @Override
    public ListConsumer setListConsumer(ListConsumer consumer) {
        mListConsumer = consumer;
        return mListConsumer;
    }

    @Override
    public void loadMorePages() {
        mCurrentPageIndex++;
    }

    @Override
    public void reset() {
        mCurrentPageIndex = 0;
    }

    @Override
    public void onListUpdated(List<ListItem> inputList) {
        if (mListConsumer == null) return;
        mListConsumer.onListUpdated(getPaginatedList(inputList));
    }

    /**
     * Given an input list, generates an output list to be displayed with a pagination header at
     * the end.
     */
    private List<ListItem> getPaginatedList(List<ListItem> inputList) {
        List<ListItem> outputList = new ArrayList<>();

        boolean showPagination = false;
        for (ListItem item : inputList) {
            boolean isDateHeader = item instanceof ListItem.SectionHeaderListItem;
            if (isDateHeader) {
                if (outputList.size() >= (mCurrentPageIndex + 1) * DEFAULT_PAGE_SIZE) {
                    showPagination = true;
                    break;
                }
            }
            outputList.add(item);
        }

        if (showPagination) {
            outputList.add(new ListItem.PaginationListItem());
            mCurrentPageIndex = outputList.size() / DEFAULT_PAGE_SIZE - 1;
        }

        return outputList;
    }
}
