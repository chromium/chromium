// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.metrics;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.util.Collection;

/** Utility methods related to metrics collection on download home. */
public class UmaUtils {
    /**
     * Called to log metrics about shared {@link OfflineItem}s.  Note that this relies on both
     * {@link OfflineItemFilter} and {@link Filters#FilterType} to determine what to log.
     * @param items The {@link OfflineItem}s that were shared.
     */
    public static void recordItemsShared(Collection<OfflineItem> items) {
        for (OfflineItem item : items) {
            if (item.filter == OfflineItemFilter.PAGE) {
                RecordUserAction.record("OfflinePages.Sharing.SharePageFromDownloadHome");
            }
        }

        ShareHelper.recordShareSource(ShareHelper.ShareSourceAndroid.ANDROID_SHARE_SHEET);
    }

    /**
     * Called to query the suffix to use when logging metrics on a per-filter type basis.
     * @param type The {@link Filters#FilterType} to convert.
     * @return     The metrics string representation of {@code type}.
     */
    public static String getSuffixForFilter(@FilterType int type) {
        switch (type) {
            case FilterType.NONE:
                return "All";
            case FilterType.SITES:
                return "OfflinePage";
            case FilterType.VIDEOS:
                return "Video";
            case FilterType.MUSIC:
                return "Audio";
            case FilterType.IMAGES:
                return "Image";
            case FilterType.DOCUMENT:
                return "Document";
            case FilterType.OTHER:
                return "Other";
            case FilterType.PREFETCHED:
                // For metrics purposes, assume all prefetched content is related to offline pages.
                return "PrefetchedOfflinePage";
            default:
                assert false : "Unexpected type " + type + " passed to getSuffixForFilter.";
                return "Invalid";
        }
    }

    /**
     * Records the required stretch for each dimension before rendering the image.
     * @param requiredWidthStretch Required stretch for width.
     * @param requiredHeightStretch Required stretch for height.
     * @param filter The filter type of the view being shown.
     */
    public static void recordImageViewRequiredStretch(
            float requiredWidthStretch,
            float requiredHeightStretch,
            @Filters.FilterType int filter) {
        float maxRequiredStretch = Math.max(requiredWidthStretch, requiredHeightStretch);
        RecordHistogram.recordCustomCountHistogram(
                "Android.DownloadManager.Thumbnail.MaxRequiredStretch."
                        + getSuffixForFilter(filter),
                (int) (maxRequiredStretch * 100),
                10,
                1000,
                50);
    }
}
