// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.metrics;

import android.os.Handler;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Helper class to log UMA based on the initial retrieved data set of {@link OfflineItem}s.  This
 * class will observe the passed in {@link OfflineItemFilterSource} until {@link OfflineItem}s are
 * available and then log the items and remove itself.
 *
 * Note that based on {@link DownloadManagerUiConfig} this class may or may not actually log data.
 */
public class OfflineItemStartupLogger implements OfflineItemFilterObserver {
    private final Handler mHandler = new Handler();

    private final boolean mAllowedToLog;
    private final OfflineItemFilterSource mSource;

    /** Creates a new {@link OfflineItemStartupLogger} instance. */
    public OfflineItemStartupLogger(
            DownloadManagerUiConfig config, OfflineItemFilterSource source) {
        mAllowedToLog = !config.isOffTheRecord;
        mSource = source;
        mSource.addObserver(this);

        if (mSource.areItemsAvailable()) onItemsAvailable();
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAvailable() {
        mSource.removeObserver(this);

        // Post to make sure the source properly adds the items.
        mHandler.post(this ::grabMetrics);
    }

    private void grabMetrics() {
        if (!mAllowedToLog) return;

        Map<Integer /* Filters.FilterType */, Integer /* Count */> counts = new HashMap<>();
        Map<Integer /* Filters.FilterType */, Integer /* Count */> viewedCounts = new HashMap<>();
        Map<Integer /* Filters.FilterType */, Integer /* Count */> otherExtensions =
                new HashMap<>();

        // Prepopulate all Filter counts with 0.  That way we properly get the metrics that show us
        // which types have no entries at all.  This also is assumed behavior by the rest of the
        // code, which does not check if the key is present.
        for (int i = 0; i < Filters.FilterType.NUM_ENTRIES; i++) {
            counts.put(i, 0);
            viewedCounts.put(i, 0);
        }

        for (OfflineItem item : mSource.getItems()) {
            @Filters.FilterType
            int type = Filters.fromOfflineItem(item);

            counts.put(type, counts.get(type) + 1);

            if (item.lastAccessedTimeMs > item.completionTimeMs) {
                viewedCounts.put(type, viewedCounts.get(type) + 1);
            }

            if (type == Filters.FilterType.OTHER) {
                @FileExtensions.Type
                int extension = FileExtensions.getExtension(item.filePath);
                if (!otherExtensions.containsKey(extension)) otherExtensions.put(extension, 0);
                otherExtensions.put(extension, otherExtensions.get(extension) + 1);
            }
        }

        RecordHistogram.recordCountHistogram(
                "Android.DownloadManager.InitialCount.Total", mSource.getItems().size());

        for (Entry<Integer /* FileExtensions.Type */, Integer /* Count */> count :
                otherExtensions.entrySet()) {
            for (int i = 0; i < count.getValue(); i++) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.DownloadManager.OtherExtensions.InitialCount", count.getKey(),
                        FileExtensions.Type.NUM_ENTRIES);
            }
        }

        for (Entry<Integer /* Filters.FilterType */, Integer /* Count */> count :
                counts.entrySet()) {
            RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount."
                            + UmaUtils.getSuffixForFilter(count.getKey()),
                    count.getValue());
        }

        for (Entry<Integer /* Filters.FilterType */, Integer /* Count */> count :
                viewedCounts.entrySet()) {
            RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Viewed."
                            + UmaUtils.getSuffixForFilter(count.getKey()),
                    count.getValue());
        }
    }
}