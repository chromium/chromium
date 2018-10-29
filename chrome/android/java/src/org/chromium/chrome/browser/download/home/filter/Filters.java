// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.support.annotation.IntDef;
import android.text.TextUtils;

import org.chromium.chrome.browser.UrlConstants;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper containing a list of Downloads Home filter types and conversion methods. */
public class Filters {
    // These statics are used for UMA logging. Please update the AndroidDownloadFilterType enum in
    // histograms.xml if these change.
    /**
     * A list of possible filter types on offlined items. Note that not all of these will show
     * up in the UI.
     *
     * As you add or remove entries from this list, please also update
     * ListUtils#FILTER_TYPE_ORDER_LIST to specify what order the sections should appear in.
     */
    @IntDef({FilterType.NONE, FilterType.SITES, FilterType.VIDEOS, FilterType.MUSIC,
            FilterType.IMAGES, FilterType.DOCUMENT, FilterType.OTHER, FilterType.PREFETCHED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FilterType {
        int NONE = 0;
        int SITES = 1;
        int VIDEOS = 2;
        int MUSIC = 3;
        int IMAGES = 4;
        int DOCUMENT = 5;
        int OTHER = 6;
        int PREFETCHED = 7;
        int NUM_ENTRIES = 8;
    }

    /**
     * Converts from a {@link OfflineItem} to a {@link FilterType}.  Note that not all
     * {@link OfflineItem}s have a corresponding match and may return {@link #NONE}
     * as they don't correspond to any UI filter.
     *
     * @param item The {@link OfflineItem} to convert.
     * @return     The corresponding {@link FilterType}.
     */
    public static @FilterType Integer fromOfflineItem(OfflineItem item) {
        if (item.isSuggested) return FilterType.PREFETCHED;

        return fromOfflineItem(item.filter);
    }

    /**
     * A subset of {@link #fromOfflineItem(OfflineItem)} that only uses {@link OfflineItem#filter}
     * to make the decision on which type of filter to use.  This will not be comprehensive for all
     * {@link OfflineItem}s.
     * @param filter The {@link OfflineItem#filter} type to convert.
     * @return       The corresponding {@link FilterType}.
     */
    public static @FilterType Integer fromOfflineItem(@OfflineItemFilter int filter) {
        switch (filter) {
            case OfflineItemFilter.FILTER_PAGE:
                return FilterType.SITES;
            case OfflineItemFilter.FILTER_VIDEO:
                return FilterType.VIDEOS;
            case OfflineItemFilter.FILTER_AUDIO:
                return FilterType.MUSIC;
            case OfflineItemFilter.FILTER_IMAGE:
                return FilterType.IMAGES;
            // case OfflineItemFilter.FILTER_OTHER
            // case OfflineItemFilter.FILTER_DOCUMENT
            default:
                return FilterType.OTHER;
        }
    }

    /**
     * Converts {@code filter} into a url.
     * @see DownloadFilter#getUrlForFilter(int)
     */
    public static String toUrl(@FilterType int filter) {
        return filter == FilterType.NONE ? UrlConstants.DOWNLOADS_URL
                                         : UrlConstants.DOWNLOADS_FILTER_URL + filter;
    }

    /**
     * Converts {@code url} to a {@link FilterType}.
     * @see DownloadFilter#getFilterFromUrl(String)
     */
    public static @FilterType int fromUrl(String url) {
        if (TextUtils.isEmpty(url) || !url.startsWith(UrlConstants.DOWNLOADS_FILTER_URL)) {
            return FilterType.NONE;
        }

        @FilterType
        int filter = FilterType.NONE;
        try {
            filter = Integer.parseInt(url.substring(UrlConstants.DOWNLOADS_FILTER_URL.length()));
            if (filter < 0 || filter >= FilterType.NUM_ENTRIES) filter = FilterType.NONE;
        } catch (NumberFormatException ex) {
        }

        return filter;
    }

    private Filters() {}
}