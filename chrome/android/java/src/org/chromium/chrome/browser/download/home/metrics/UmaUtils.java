// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.metrics;

import android.support.annotation.IdRes;
import android.support.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.download.R;
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

    // Please treat this list as append only and keep it in sync with
    // Android.DownloadManager.List.View.Actions in enums.xml.
    @IntDef({ViewAction.OPEN, ViewAction.RESUME, ViewAction.PAUSE, ViewAction.CANCEL,
            ViewAction.MENU_SHARE, ViewAction.MENU_DELETE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewAction {
        int OPEN = 0;
        int RESUME = 1;
        int PAUSE = 2;
        int CANCEL = 3;
        int MENU_SHARE = 4;
        int MENU_DELETE = 5;
        int NUM_ENTRIES = 6;
    }

    // Please treat this list as append only and keep it in sync with
    // Android.DownloadManager.List.Section.Menu.Actions in enums.xml.
    @IntDef({ImagesMenuAction.MENU_START_SELECTING, ImagesMenuAction.MENU_SHARE_ALL,
            ImagesMenuAction.MENU_DELETE_ALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ImagesMenuAction {
        int MENU_START_SELECTING = 0;
        int MENU_SHARE_ALL = 1;
        int MENU_DELETE_ALL = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Called to record metrics for the given images section menu action.
     * @param action The given menu action.
     */
    public static void recordImagesMenuAction(@ImagesMenuAction int action) {
        String userActionSuffix;
        switch (action) {
            case ImagesMenuAction.MENU_START_SELECTING:
                userActionSuffix = "StartSelecting";
                break;
            case ImagesMenuAction.MENU_SHARE_ALL:
                userActionSuffix = "ShareAll";
                break;
            case ImagesMenuAction.MENU_DELETE_ALL:
                userActionSuffix = "DeleteAll";
                break;
            default:
                assert false : "Unexpected action " + action + " passed to recordImagesMenuAction.";
                return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.List.Section.Menu.Images.Action", action,
                ImagesMenuAction.NUM_ENTRIES);
        RecordUserAction.record(
                "Android.DownloadManager.List.Selection.Menu.Images.Action." + userActionSuffix);
    }

    /**
     * Called to record metrics for the given list item action.
     * @param action The given list item action.
     */
    public static void recordItemAction(@ViewAction int action) {
        String userActionSuffix;
        switch (action) {
            case ViewAction.OPEN:
                userActionSuffix = "Open";
                break;
            case ViewAction.RESUME:
                userActionSuffix = "Resume";
                break;
            case ViewAction.PAUSE:
                userActionSuffix = "Pause";
                break;
            case ViewAction.CANCEL:
                userActionSuffix = "Cancel";
                break;
            case ViewAction.MENU_SHARE:
                userActionSuffix = "MenuShare";
                break;
            case ViewAction.MENU_DELETE:
                userActionSuffix = "MenuDelete";
                break;
            default:
                assert false : "Unexpected action " + action + " passed to recordItemAction.";
                return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.List.View.Action", action, ViewAction.NUM_ENTRIES);
        RecordUserAction.record("Android.DownloadManager.List.View.Action." + userActionSuffix);
    }

    /**
     * Called to record metrics for the given top menu action.
     * @param menuId The menu item id that the user interacted with.
     */
    public static void recordTopMenuAction(@IdRes int menuId) {
        @MenuAction
        int action;
        String userActionSuffix;

        if (menuId == R.id.close_menu_id || menuId == R.id.with_settings_close_menu_id) {
            action = MenuAction.CLOSE;
            userActionSuffix = "Close";
        } else if (menuId == R.id.selection_mode_delete_menu_id) {
            action = MenuAction.MULTI_DELETE;
            userActionSuffix = "MultiDelete";
        } else if (menuId == R.id.selection_mode_share_menu_id) {
            action = MenuAction.MULTI_SHARE;
            userActionSuffix = "MultiShare";
        } else if (menuId == R.id.with_settings_search_menu_id || menuId == R.id.search_menu_id) {
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
     * Called to log the number of items that were shared through the multi-share top menu action.
     * @param count The number of items that were shared.
     */
    public static void recordTopMenuShareCount(int count) {
        RecordHistogram.recordCount100Histogram(
                "Android.DownloadManager.Menu.Share.SelectedCount", count);
    }

    /**
     * Called to log the number of items that were deleted through the multi-delete top menu action.
     * @param count The number of items that were deleted.
     */
    public static void recordTopMenuDeleteCount(int count) {
        RecordHistogram.recordCount100Histogram(
                "Android.DownloadManager.Menu.Delete.SelectedCount", count);
    }

    /**
     * Called to log metrics about shared {@link OfflineItem}s.  Note that this relies on both
     * {@link OfflineItemFilter} and {@link Filters#FilterType} to determine what to log.
     * @param items The {@link OfflineItem}s that were shared.
     */
    public static void recordItemsShared(Collection<OfflineItem> items) {
        for (OfflineItem item : items) {
            if (item.filter == OfflineItemFilter.FILTER_PAGE) {
                RecordUserAction.record("OfflinePages.Sharing.SharePageFromDownloadHome");
            }

            @Filters.FilterType
            int filterType = Filters.fromOfflineItem(item);

            if (filterType == Filters.FilterType.OTHER) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.DownloadManager.OtherExtensions.Share",
                        FileExtensions.getExtension(item.filePath),
                        FileExtensions.Type.NUM_ENTRIES);
            }

            RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Share.FileTypes",
                    filterType, Filters.FilterType.NUM_ENTRIES);
        }

        RecordHistogram.recordLinearCountHistogram(
                "Android.DownloadManager.Share.Count", items.size(), 1, 20, 20);
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
}
