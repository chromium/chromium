// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import android.util.Pair;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.CardDividerListItem;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Given a sorted list of offline items, generates a list of {@link ListItem} that has card header
 * and footer items for group cards.
 */
public class GroupCardLabelAdder implements ListConsumer {
    private static final long CARD_DIVIDER_MIDDLE_HASH_CODE_OFFSET = 200000;

    private ListConsumer mListConsumer;
    private CardPaginator mCardPaginator;
    private long mDividerIndexId;

    /** Constructor. */
    public GroupCardLabelAdder(CardPaginator paginator) {
        mCardPaginator = paginator;
    }

    @Override
    public ListConsumer setListConsumer(ListConsumer consumer) {
        mListConsumer = consumer;
        return mListConsumer;
    }

    @Override
    public void onListUpdated(List<ListItem> inputList) {
        if (mListConsumer == null) return;
        mListConsumer.onListUpdated(addLabels(inputList));
    }

    private List<ListItem> addLabels(List<ListItem> sortedList) {
        mDividerIndexId = CARD_DIVIDER_MIDDLE_HASH_CODE_OFFSET;

        List<ListItem> outList = new ArrayList<>();
        List<ListItem> candidateCardItems = new ArrayList<>();
        ListItem previousItem = null;
        for (ListItem listItem : sortedList) {
            if (listItem instanceof ListItem.OfflineItemListItem) {
                ((ListItem.OfflineItemListItem) listItem).isGrouped = false;
            }

            boolean addToExistingGroup =
                    ListUtils.canGroup(listItem)
                            && ListUtils.canGroup(previousItem)
                            && getDateAndDomainForItem(listItem)
                                    .equals(getDateAndDomainForItem(previousItem));
            if (addToExistingGroup) {
                candidateCardItems.add(listItem);
            } else {
                // Add the grouped items that we have seen so far but didn't add into a card.
                flushCandidateCardItemsToList(candidateCardItems, outList);
                candidateCardItems.clear();

                if (ListUtils.canGroup(listItem)) {
                    // The item is content indexed with a different domain. Start a new group.
                    candidateCardItems.add(listItem);
                } else {
                    // The item is not content indexed. Just add it to the list.
                    outList.add(listItem);
                }
            }
            previousItem = listItem;
        }

        flushCandidateCardItemsToList(candidateCardItems, outList);

        return outList;
    }

    /**
     * Flushes the candidate card items into the list. If the item count is greater than minimum
     * threshold, creates a group card out of the items, otherwise adds them to the flat list. Adds
     * the header and footer cards for the group card.
     * @param candidateCardItems The items to be considered for creating a group card. They must
     *         have same domain.
     * @param outList The output list that would be shown in the UI.
     */
    private void flushCandidateCardItemsToList(
            List<ListItem> candidateCardItems, List<ListItem> outList) {
        if (candidateCardItems.isEmpty()) return;

        if (candidateCardItems.size() < mCardPaginator.minItemCountPerCard()) {
            // We don't have enough items to build a card. Just add them to the flat list.
            outList.addAll(candidateCardItems);
            return;
        }

        Pair<Date, String> dateAndDomain = getDateAndDomainForItem(candidateCardItems.get(0));
        String url = ((ListItem.OfflineItemListItem) candidateCardItems.get(0)).item.url.getSpec();
        mCardPaginator.initializeEntry(dateAndDomain);

        // Add the card header, and the divider above it.
        outList.add(createDivider(CardDividerListItem.Position.TOP));
        outList.add(new ListItem.CardHeaderListItem(dateAndDomain, url));

        int itemsBeforePagination = mCardPaginator.getItemCountForCard(dateAndDomain);
        int numItemsToShow = Math.min(itemsBeforePagination, candidateCardItems.size());
        // Add the list items and the associated dividers.
        for (int i = 0; i < numItemsToShow; i++) {
            ListItem.OfflineItemListItem listItem =
                    (ListItem.OfflineItemListItem) candidateCardItems.get(i);
            listItem.isGrouped = true;

            outList.add(listItem);
            if (i < numItemsToShow - 1) {
                outList.add(createDivider(CardDividerListItem.Position.MIDDLE));
            }
        }

        if (candidateCardItems.size() > itemsBeforePagination) {
            // Add the card footer and the divider below.
            outList.add(createDivider(CardDividerListItem.Position.MIDDLE));
            outList.add(new ListItem.CardFooterListItem(dateAndDomain));
        }

        outList.add(createDivider(CardDividerListItem.Position.BOTTOM));
    }

    private ListItem createDivider(CardDividerListItem.Position position) {
        return new CardDividerListItem(mDividerIndexId++, position);
    }

    private static Pair<Date, String> getDateAndDomainForItem(ListItem listItem) {
        assert ListUtils.canGroup(listItem);
        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) listItem).item;
        Date date = CalendarUtils.getStartOfDay(offlineItem.creationTimeMs).getTime();
        String domain = UiUtils.getDomainForItem(offlineItem);
        return Pair.create(date, domain);
    }
}
