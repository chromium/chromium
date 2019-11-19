// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles pagination for the list and adds a pagination header at the end, if the list is longer
 * than the desired length. Tracks the number of pages currently being displayed to the user.
 */
public class Paginator {
    private static final int DEFAULT_PAGE_SIZE = 25;

    private int mCurrentPageIndex;

    /** Constructor. */
    public Paginator() {}

    /**
     * Increments the currently displayed page count. Called when the pagination header is clicked.
     */
    public void loadMorePages() {
        mCurrentPageIndex++;
    }

    /**
     * Given an input list, generates an output list to be displayed with a pagination header at
     * the end.
     */
    public List<ListItem> getPaginatedList(List<ListItem> inputList) {
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

        if (showPagination) outputList.add(new ListItem.PaginationListItem());

        return outputList;
    }

    /**
     * Resets the pagination tracking. To be called when the filter type of the list is changed.
     */
    public void reset() {
        mCurrentPageIndex = 0;
    }
}
