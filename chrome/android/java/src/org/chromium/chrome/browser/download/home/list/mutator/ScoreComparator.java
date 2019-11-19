// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.list.ListUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Comparator;

/**
 * Comparator based on score. Comparison is done first based on the score. For items having same
 * score, filter type and timestamp will be used to further compare.
 */
public class ScoreComparator implements Comparator<OfflineItem> {
    @Override
    public int compare(OfflineItem lhs, OfflineItem rhs) {
        int comparison = compareItemByScore(lhs, rhs);
        if (comparison != 0) return comparison;

        comparison = ListUtils.compareFilterTypesTo(
                Filters.fromOfflineItem(lhs), Filters.fromOfflineItem(rhs));
        if (comparison != 0) return comparison;

        comparison = ListUtils.compareItemByTimestamp(lhs, rhs);
        if (comparison != 0) return comparison;

        return ListUtils.compareItemByID(lhs, rhs);
    }

    private int compareItemByScore(OfflineItem lhs, OfflineItem rhs) {
        return Double.compare(rhs.contentQualityScore, lhs.contentQualityScore);
    }
}
