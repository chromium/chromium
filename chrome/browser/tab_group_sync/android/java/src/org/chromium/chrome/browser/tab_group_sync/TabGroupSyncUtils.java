// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.EventDetails;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupEvent;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;

/** Utility methods for tab group sync. */
@NullMarked
public final class TabGroupSyncUtils {
    // The URL written to sync when the local URL isn't in a syncable format, i.e. HTTP or HTTPS.
    public static final GURL UNSAVEABLE_URL_OVERRIDE = new GURL(UrlConstants.NTP_NON_NATIVE_URL);
    public static final String UNSAVEABLE_TAB_TITLE = "Unsavable tab";
    public static final GURL NTP_URL = new GURL(UrlConstants.NTP_NON_NATIVE_URL);
    public static final String NEW_TAB_TITLE = "New tab";

    /**
     * Whether the given {@param localId} corresponds to a tab group in the current window
     * corresponding to {@param tabGroupModelFilter}.
     *
     * @param tabGroupModelFilter The tab group model filter in which to find the tab group.
     * @param localId The ID of the tab group.
     */
    public static boolean isInCurrentWindow(
            TabGroupModelFilter tabGroupModelFilter, LocalTabGroupId localId) {
        return tabGroupModelFilter.tabGroupExists(localId.tabGroupId);
    }

    private static boolean isInAnyWindow(
            LocalTabGroupId localId, List<TabGroupModelFilter> filterList) {
        for (TabGroupModelFilter filter : filterList) {
            if (isInCurrentWindow(filter, localId)) {
                return true;
            }
        }
        return false;
    }

    /** Conversion method to get a {@link LocalTabGroupId} from a root ID. */
    public static @Nullable LocalTabGroupId getLocalTabGroupId(
            TabGroupModelFilter filter, @Nullable Token tabGroupId) {
        if (tabGroupId == null || !filter.tabGroupExists(tabGroupId)) return null;
        return new LocalTabGroupId(tabGroupId);
    }

    /** Util method to get a {@link LocalTabGroupId} from a tab. */
    public static @Nullable LocalTabGroupId getLocalTabGroupId(Tab tab) {
        Token tabGroupId = tab.getTabGroupId();
        return tabGroupId == null ? null : new LocalTabGroupId(tabGroupId);
    }

    /** Utility method to filter out URLs not suitable for tab group sync. */
    public static Pair<GURL, String> getFilteredUrlAndTitle(GURL url, String title) {
        assert url != null;
        if (isSavableUrl(url)) {
            return new Pair<>(url, title);
        } else if (isNtpOrAboutBlankUrl(url)) {
            return new Pair<>(NTP_URL, NEW_TAB_TITLE);
        } else {
            return new Pair<>(UNSAVEABLE_URL_OVERRIDE, UNSAVEABLE_TAB_TITLE);
        }
    }

    /** Utility method to determine if a URL can be synced or not. */
    public static boolean isSavableUrl(GURL url) {
        return UrlUtilities.isHttpOrHttps(url);
    }

    @VisibleForTesting
    static boolean isNtpOrAboutBlankUrl(GURL url) {
        String urlString = url.getValidSpecOrEmpty();
        return UrlUtilities.isNtpUrl(url)
                || TextUtils.equals(urlString, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)
                || TextUtils.equals(urlString, ContentUrlConstants.ABOUT_BLANK_URL);
    }

    /**
     * Removes all tab groups mappings found in the {@link TabGroupSyncService} that don't have
     * corresponding local IDs in the {@link TabGroupModelFilter}.
     *
     * @param tabGroupSyncService The {@link TabGroupSyncService} to remove tabs from.
     * @param filter The {@link TabGroupModelFilter} to check for tab groups.
     */
    public static void unmapLocalIdsNotInTabGroupModelFilter(
            TabGroupSyncService tabGroupSyncService, TabGroupModelFilter filter) {
        unmapLocalIdsNotInTabGroupModelFilterList(
                tabGroupSyncService, Collections.singletonList(filter));
    }

    /** Same as {@Link #unmapLocalIdsNotInTabGroupModelFilter} only with a list of filters. */
    public static void unmapLocalIdsNotInTabGroupModelFilterList(
            TabGroupSyncService tabGroupSyncService, List<TabGroupModelFilter> filterList) {
        for (TabGroupModelFilter filter : filterList) {
            assert !filter.getTabModel().isOffTheRecord();
        }

        for (String syncGroupId : tabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(syncGroupId);
            // If there is no local ID the group is already hidden so this is a no-op.
            if (savedTabGroup == null || savedTabGroup.localId == null) continue;

            if (!isInAnyWindow(savedTabGroup.localId, filterList)) {
                tabGroupSyncService.removeLocalTabGroupMapping(
                        savedTabGroup.localId, ClosingSource.CLEANED_UP_ON_LAST_INSTANCE_CLOSURE);
            }
        }
    }

    /** Helper method to record open close metrics. */
    public static void recordTabGroupOpenCloseMetrics(
            TabGroupSyncService tabGroupSyncService,
            boolean open,
            int source,
            LocalTabGroupId localTabGroupId) {
        int eventType = open ? TabGroupEvent.TAB_GROUP_OPENED : TabGroupEvent.TAB_GROUP_CLOSED;
        EventDetails eventDetails = new EventDetails(eventType);
        eventDetails.localGroupId = localTabGroupId;
        if (open) {
            eventDetails.openingSource = source;
        } else {
            eventDetails.closingSource = source;
        }
        tabGroupSyncService.recordTabGroupEvent(eventDetails);
    }

    /**
     * Tries to get the saved group for a given tab id, returning null if anything goes wrong, such
     * as the tab not being in a group.
     *
     * @param tabId The id of the tab.
     * @param tabModel The tab model to look up the tab in.
     * @param tabGroupSyncService The sync service to get tab group data form.
     * @return The group data object.
     */
    public static @Nullable SavedTabGroup getSavedTabGroupFromTabId(
            int tabId, TabModel tabModel, TabGroupSyncService tabGroupSyncService) {
        @Nullable Tab tab = tabModel.getTabById(tabId);
        if (tab == null || tab.getTabGroupId() == null) return null;
        LocalTabGroupId localTabGroupId = new LocalTabGroupId(tab.getTabGroupId());
        return tabGroupSyncService.getGroup(localTabGroupId);
    }

    /**
     * Returns the last access time of a tab group which is determined by most recent access time
     * across all of its tabs.
     *
     * @param tabGroupId The local tab group ID.
     * @param tabGroupModelFilter The tab group model filter.
     * @return The last access time of the tab group.
     */
    public static long getTabGroupLastAccessTime(
            Token tabGroupId, TabGroupModelFilter tabGroupModelFilter) {
        List<Tab> tabs = tabGroupModelFilter.getTabsInGroup(tabGroupId);
        long mostRecentAccessTime = 0;
        for (Tab tab : tabs) {
            mostRecentAccessTime = Math.max(mostRecentAccessTime, tab.getTimestampMillis());
        }

        return mostRecentAccessTime;
    }

    /**
     * Called to when a navigation finishes in the tab.
     *
     * @param tab Tab that triggers the navigation.
     * @param navigationHandle Navigation handle to retrieve the redirect chain from.
     */
    public static void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
        LocalTabGroupId localTabGroupId = getLocalTabGroupId(tab);
        if (localTabGroupId == null) return;
        TabGroupSyncUtilsJni.get()
                .onDidFinishNavigation(
                        tab.getProfile(),
                        localTabGroupId,
                        tab.getId(),
                        navigationHandle.nativeNavigationHandlePtr());
    }

    /**
     * Called to update the tab redirect chain.
     *
     * @param tab Tab that triggers the navigation.
     * @param navigationHandle Navigation handle to retrieve the redirect chain from.
     */
    public static void updateTabRedirectChain(Tab tab, NavigationHandle navigationHandle) {
        LocalTabGroupId localTabGroupId = getLocalTabGroupId(tab);
        if (localTabGroupId == null) return;
        TabGroupSyncUtilsJni.get()
                .updateTabRedirectChain(
                        tab.getProfile(),
                        localTabGroupId,
                        tab.getId(),
                        navigationHandle.nativeNavigationHandlePtr());
    }

    /**
     * Called to check if a URL is part of the redirect chain of the current tab URL.
     *
     * @param tab Tab whose URL redirect chain needs to be checked.
     * @param url The URL to be checked.
     * @return true if the URL belongs to the tab's redirect chain, or false otherwise.
     */
    public static boolean isUrlInTabRedirectChain(Tab tab, GURL url) {
        LocalTabGroupId localTabGroupId = getLocalTabGroupId(tab);
        if (localTabGroupId == null) return false;
        return TabGroupSyncUtilsJni.get()
                .isUrlInTabRedirectChain(tab.getProfile(), localTabGroupId, tab.getId(), url);
    }

    /**
     * Called to check whether the navigation can be saved to sync.
     *
     * @param isExtensionNavigationAllowed Whether navigation from extension is allowed.
     * @param navigationHandle Navigation handle associated with the navigation.
     */
    public static boolean isSaveableNavigation(
            boolean isExtensionNavigationAllowed, NavigationHandle navigationHandle) {
        return TabGroupSyncUtilsJni.get()
                .isSaveableNavigation(
                        isExtensionNavigationAllowed, navigationHandle.nativeNavigationHandlePtr());
    }

    @NativeMethods
    interface Natives {
        void onDidFinishNavigation(
                @JniType("Profile*") Profile profile,
                LocalTabGroupId groupId,
                int tabId,
                long navigationHandlePtr);

        void updateTabRedirectChain(
                @JniType("Profile*") Profile profile,
                LocalTabGroupId groupId,
                int tabId,
                long navigationHandlePtr);

        boolean isUrlInTabRedirectChain(
                @JniType("Profile*") Profile profile,
                LocalTabGroupId groupId,
                int tabId,
                @JniType("GURL") GURL url);

        boolean isSaveableNavigation(
                boolean isExtensionNavigationAllowed, long navigationHandlePtr);
    }
}
