// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SectionHeaderType;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Given a list of {@link ListItem}, returns a list that has date headers for each date. Also adds
 * Just Now header for recently completed items. Note that this class must be called on the list
 * before adding any other labels such as card header/footer/pagination etc.
 */
public class DateLabelAdder implements ListConsumer {
    @Nullable private final JustNowProvider mJustNowProvider;
    private ListConsumer mListConsumer;

    public DateLabelAdder(
            DownloadManagerUiConfig config, @Nullable JustNowProvider justNowProvider) {
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
        mListConsumer.onListUpdated(addLabels(inputList));
    }

    private List<ListItem> addLabels(List<ListItem> sortedList) {
        List<ListItem> listWithHeaders = new ArrayList<>();

        // Insert section headers to the list and output the new list with headers.
        OfflineItem previousItem = null;
        for (int i = 0; i < sortedList.size(); i++) {
            ListItem listItem = sortedList.get(i);
            if (!(listItem instanceof OfflineItemListItem)) continue;

            OfflineItem currentItem = ((OfflineItemListItem) listItem).item;
            maybeAddSectionHeader(listWithHeaders, currentItem, previousItem);
            listWithHeaders.add(listItem);
            previousItem = currentItem;
        }

        return listWithHeaders;
    }

    private void maybeAddSectionHeader(
            List<ListItem> listWithHeaders,
            @NonNull OfflineItem currentItem,
            @Nullable OfflineItem previousItem) {
        @SectionHeaderType int currentHeaderType = getSectionHeaderType(currentItem);
        @SectionHeaderType int previousHeaderType = getSectionHeaderType(previousItem);

        // Add a section header when starting a new section.
        if (currentHeaderType != previousHeaderType) {
            addSectionHeader(listWithHeaders, currentItem);
            return;
        }

        // For date time section, each day has a header.
        if (currentHeaderType == SectionHeaderType.DATE
                && startOfNewDay(currentItem, previousItem)) {
            addSectionHeader(listWithHeaders, currentItem);
            return;
        }
    }

    private void addSectionHeader(
            List<ListItem> listWithHeaders, @NonNull OfflineItem currentItem) {
        Date day = CalendarUtils.getStartOfDay(currentItem.creationTimeMs).getTime();
        ListItem.SectionHeaderListItem sectionHeaderItem =
                new ListItem.SectionHeaderListItem(
                        day.getTime(), getSectionHeaderType(currentItem));
        listWithHeaders.add(sectionHeaderItem);
    }

    private @SectionHeaderType int getSectionHeaderType(@Nullable OfflineItem offlineItem) {
        if (offlineItem == null) return SectionHeaderType.INVALID;

        // Just now section follows the scheduled for later section.
        boolean isJustNow = mJustNowProvider != null && mJustNowProvider.isJustNowItem(offlineItem);
        if (isJustNow) return SectionHeaderType.JUST_NOW;

        // Regular section that shows date as the section header.
        return SectionHeaderType.DATE;
    }

    private static boolean startOfNewDay(
            OfflineItem currentItem, @Nullable OfflineItem previousItem) {
        Date currentDay = CalendarUtils.getStartOfDay(currentItem.creationTimeMs).getTime();
        Date previousDay =
                previousItem == null
                        ? null
                        : CalendarUtils.getStartOfDay(previousItem.creationTimeMs).getTime();
        return !currentDay.equals(previousDay);
    }
}
