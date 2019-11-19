// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Comparator;

/**
 * Comparator based on download date. Items having same date (day of download creation), will be
 * compared based on mime type. For further tie-breakers, timestamp and ID are used.
 */
public class DateComparator implements Comparator<OfflineItem> {
    private final JustNowProvider mJustNowProvider;

    public DateComparator(JustNowProvider justNowProvider) {
        mJustNowProvider = justNowProvider;
    }

    @Override
    public int compare(OfflineItem lhs, OfflineItem rhs) {
        int comparison = compareItemByJustNowProvider(lhs, rhs);
        if (comparison != 0) return comparison;

        comparison = ListUtils.compareItemByDate(lhs, rhs);
        if (comparison != 0) return comparison;

        comparison = ListUtils.compareFilterTypesTo(
                Filters.fromOfflineItem(lhs), Filters.fromOfflineItem(rhs));
        if (comparison != 0) return comparison;

        comparison = ListUtils.compareItemByTimestamp(lhs, rhs);
        if (comparison != 0) return comparison;

        return ListUtils.compareItemByID(lhs, rhs);
    }

    private int compareItemByJustNowProvider(OfflineItem lhs, OfflineItem rhs) {
        boolean lhsIsJustNowItem = mJustNowProvider.isJustNowItem(lhs);
        boolean rhsIsJustNowItem = mJustNowProvider.isJustNowItem(rhs);
        if (lhsIsJustNowItem == rhsIsJustNowItem) return 0;
        return lhsIsJustNowItem && !rhsIsJustNowItem ? -1 : 1;
    }
}
