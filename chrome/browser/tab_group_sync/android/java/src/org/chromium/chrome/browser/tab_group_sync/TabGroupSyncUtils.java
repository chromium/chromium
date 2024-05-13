// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

/** Utility methods for tab group sync. */
public final class TabGroupSyncUtils {
    // The URL written to sync when the local URL isn't in a syncable format, i.e. HTTP or HTTPS.
    public static final GURL UNSAVEABLE_URL_OVERRIDE = new GURL(UrlConstants.NTP_NON_NATIVE_URL);
    public static final String UNSAVEABLE_TAB_TITLE = "Unsavable tab";

    /**
     * Whether the given {@param localId} corresponds to a tab group in the current window
     * corresponding to {@param tabGroupModelFilter}.
     *
     * @param tabGroupModelFilter The tab group model filter in which to find the tab group.
     * @param localId The ID of the tab group.
     */
    public static boolean isInCurrentWindow(
            TabGroupModelFilter tabGroupModelFilter, LocalTabGroupId localId) {
        int rootId = tabGroupModelFilter.getRootIdFromStableId(localId.tabGroupId);
        return rootId != Tab.INVALID_TAB_ID;
    }

    /** Conversion method to get a {@link LocalTabGroupId} from a root ID. */
    public static LocalTabGroupId getLocalTabGroupId(TabGroupModelFilter filter, int rootId) {
        Token tabGroupId = filter.getStableIdFromRootId(rootId);
        return tabGroupId == null ? null : new LocalTabGroupId(tabGroupId);
    }

    /** Conversion method to get a root ID from a {@link LocalTabGroupId}. */
    public static int getRootId(TabGroupModelFilter filter, LocalTabGroupId localTabGroupId) {
        assert localTabGroupId != null;
        return filter.getRootIdFromStableId(localTabGroupId.tabGroupId);
    }

    /** Util method to get a {@link LocalTabGroupId} from a tab. */
    public static LocalTabGroupId getLocalTabGroupId(Tab tab) {
        return new LocalTabGroupId(tab.getTabGroupId());
    }

    /** Utility method to filter out URLs not suitable for tab group sync. */
    public static Pair<GURL, String> getFilteredUrlAndTitle(GURL url, String title) {
        assert url != null;
        if (isSavableUrl(url)) {
            return new Pair<>(url, title);
        } else {
            return new Pair<>(UNSAVEABLE_URL_OVERRIDE, UNSAVEABLE_TAB_TITLE);
        }
    }

    /** Utility method to determine if a URL can be synced or not. */
    public static boolean isSavableUrl(GURL url) {
        return UrlUtilities.isHttpOrHttps(url) || isNtpOrAboutBlankUrl(url.getValidSpecOrEmpty());
    }

    @VisibleForTesting
    static boolean isNtpOrAboutBlankUrl(String url) {
        return TextUtils.equals(url, UrlConstants.NTP_URL)
                || TextUtils.equals(url, UrlConstants.NTP_NON_NATIVE_URL)
                || TextUtils.equals(url, UrlConstants.NTP_ABOUT_URL)
                || TextUtils.equals(url, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)
                || TextUtils.equals(url, ContentUrlConstants.ABOUT_BLANK_URL);
    }
}
