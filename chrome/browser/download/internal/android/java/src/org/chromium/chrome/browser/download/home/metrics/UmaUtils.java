// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.metrics;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;

/** Utility methods related to metrics collection on download home. */
public class UmaUtils {
    // Please treat this list as append only and keep it in sync with
    // Android.DownloadManager.Menu.Actions in enums.xml.
    @IntDef({MenuAction.CLOSE, MenuAction.MULTI_DELETE, MenuAction.MULTI_SHARE,
            MenuAction.SHOW_INFO, MenuAction.HIDE_INFO, MenuAction.SEARCH, MenuAction.SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MenuAction {
        int CLOSE = 0;
        int MULTI_DELETE = 1;
        int MULTI_SHARE = 2;
        int SHOW_INFO = 3;
        int HIDE_INFO = 4;
        int SEARCH = 5;
        int SETTINGS = 6;
        int NUM_ENTRIES = 7;
    }

    /**
     * Called to record metrics for the given top menu action.
     * @param menuId The menu item id that the user interacted with.
     */
    public static void recordTopMenuAction(@IdRes int menuId) {
        @MenuAction
        int action;
        String userActionSuffix;

        if (menuId == R.id.close_menu_id) {
            action = MenuAction.CLOSE;
            userActionSuffix = "Close";
        } else if (menuId == R.id.selection_mode_delete_menu_id) {
            action = MenuAction.MULTI_DELETE;
            userActionSuffix = "MultiDelete";
        } else if (menuId == R.id.selection_mode_share_menu_id) {
            action = MenuAction.MULTI_SHARE;
            userActionSuffix = "MultiShare";
        } else if (menuId == R.id.search_menu_id) {
            action = MenuAction.SEARCH;
            userActionSuffix = "Search";
        } else if (menuId == R.id.settings_menu_id) {
            action = MenuAction.SETTINGS;
            userActionSuffix = "Settings";
        } else {
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.Menu.Action", action, MenuAction.NUM_ENTRIES);
        RecordUserAction.record("Android.DownloadManager.Menu.Action.".concat(userActionSuffix));
    }

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
    public static void recordImageViewRequiredStretch(float requiredWidthStretch,
            float requiredHeightStretch, @Filters.FilterType int filter) {
        float maxRequiredStretch = Math.max(requiredWidthStretch, requiredHeightStretch);
        RecordHistogram.recordCustomCountHistogram(
                "Android.DownloadManager.Thumbnail.MaxRequiredStretch."
                        + getSuffixForFilter(filter),
                (int) (maxRequiredStretch * 100), 10, 1000, 50);
    }
}
