// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import android.support.annotation.Nullable;

import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.list.CalendarUtils;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Implementation of {@link LabelAdder} that adds date headers for each date.
 * Also adds Just Now header for recently completed items.
 */
public class DateLabelAdder implements DateOrderedListMutator.LabelAdder {
    private final DownloadManagerUiConfig mConfig;
    private final JustNowProvider mJustNowProvider;

    public DateLabelAdder(DownloadManagerUiConfig config, JustNowProvider justNowProvider) {
        mConfig = config;
        mJustNowProvider = justNowProvider;
    }

    @Override
    public List<ListItem> addLabels(List<OfflineItem> sortedList) {
        List<ListItem> listItems = new ArrayList<>();
        OfflineItem previousItem = null;
        for (int i = 0; i < sortedList.size(); i++) {
            OfflineItem offlineItem = sortedList.get(i);
            if (startOfNewDay(offlineItem, previousItem)
                    || justNowSectionsDiffer(offlineItem, previousItem)) {
                addDateHeader(listItems, offlineItem, i);
            }

            addOfflineListItem(listItems, sortedList, i);
            previousItem = offlineItem;
        }

        return listItems;
    }

    private void addOfflineListItem(
            List<ListItem> listItems, List<OfflineItem> sortedList, int index) {
        OfflineItem currentItem = sortedList.get(index);
        OfflineItemListItem offlineItemListItem = new OfflineItemListItem(currentItem);
        listItems.add(offlineItemListItem);

        if (mConfig.supportFullWidthImages && currentItem.filter == OfflineItemFilter.IMAGE) {
            markFullWidthImageIfApplicable(offlineItemListItem, sortedList, index);
        }
    }

    private void addDateHeader(List<ListItem> listItems, OfflineItem currentItem, int index) {
        Date day = CalendarUtils.getStartOfDay(currentItem.creationTimeMs).getTime();
        ListItem.SectionHeaderListItem sectionHeaderItem =
                new ListItem.SectionHeaderListItem(day.getTime(),
                        mJustNowProvider.isJustNowItem(currentItem), index != 0 /* showDivider */);
        listItems.add(sectionHeaderItem);
    }

    private static void markFullWidthImageIfApplicable(
            OfflineItemListItem offlineItemListItem, List<OfflineItem> sortedList, int index) {
        OfflineItem previousItem = index == 0 ? null : sortedList.get(index - 1);
        OfflineItem nextItem = index >= sortedList.size() - 1 ? null : sortedList.get(index + 1);
        boolean previousItemIsImage =
                previousItem != null && previousItem.filter == OfflineItemFilter.IMAGE;
        boolean nextItemIsImage = nextItem != null && nextItem.filter == OfflineItemFilter.IMAGE;
        if (!previousItemIsImage && !nextItemIsImage) offlineItemListItem.spanFullWidth = true;
    }

    private static boolean startOfNewDay(
            OfflineItem currentItem, @Nullable OfflineItem previousItem) {
        Date currentDay = CalendarUtils.getStartOfDay(currentItem.creationTimeMs).getTime();
        Date previousDay = previousItem == null
                ? null
                : CalendarUtils.getStartOfDay(previousItem.creationTimeMs).getTime();
        return !currentDay.equals(previousDay);
    }

    private boolean justNowSectionsDiffer(
            OfflineItem currentItem, @Nullable OfflineItem previousItem) {
        if (currentItem == null || previousItem == null) return true;
        return mJustNowProvider.isJustNowItem(currentItem)
                != mJustNowProvider.isJustNowItem(previousItem);
    }
}
