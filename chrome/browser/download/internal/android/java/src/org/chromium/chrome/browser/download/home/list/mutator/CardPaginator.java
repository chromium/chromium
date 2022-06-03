// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import android.util.Pair;

import java.util.Date;
import java.util.HashMap;
import java.util.Map;

/**
 * Maintains the pagination info for the group cards to be shown.
 */
public class CardPaginator {
    private static final int ITEM_COUNT_PER_PAGE = 3;

    // Maintains the current page count for each card. The cards are keyed by date and domain.
    private Map<Pair<Date, String>, Integer> mPageCountForCard = new HashMap<>();

    /**
     * Called to load one more page for the given card.
     * @param dateAndDomain The date and domain for the items in the card.
     */
    public void loadMore(Pair<Date, String> dateAndDomain) {
        mPageCountForCard.put(dateAndDomain, getCurrentPageCount(dateAndDomain) + 1);
    }

    /**
     * Called to initialize a card entry in the map if it doesn't already exist.
     * @param key The date and domain associated with the card.
     */
    public void initializeEntry(Pair<Date, String> key) {
        if (mPageCountForCard.containsKey(key)) return;
        mPageCountForCard.put(key, 1);
    }

    /**
     * Called to get the item count on the card.
     * @param dateAndDomain The date and domain for the items in the card.
     * @return The number of items being shown on the card.
     */
    public int getItemCountForCard(Pair<Date, String> dateAndDomain) {
        return getCurrentPageCount(dateAndDomain) * ITEM_COUNT_PER_PAGE;
    }

    /**
     * @return The minimum number of items to be shown on a group card. If item count is less than
     *         this number, group card cannot be used.
     */
    public int minItemCountPerCard() {
        return ITEM_COUNT_PER_PAGE;
    }

    /**
     * Called to reset the item count on the card.
     */
    public void reset() {
        mPageCountForCard.clear();
    }

    /** @return The number of pages currently being displayed. Default is 1.*/
    private int getCurrentPageCount(Pair<Date, String> dateAndDomain) {
        return mPageCountForCard.containsKey(dateAndDomain) ? mPageCountForCard.get(dateAndDomain)
                                                            : 1;
    }
}
