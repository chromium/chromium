// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.TimeUtils;
import org.chromium.base.Token;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
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

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Utility methods for tab group sync. */
public final class TabGroupSyncUtils {
    // The URL written to sync when the local URL isn't in a syncable format, i.e. HTTP or HTTPS.
    public static final GURL UNSAVEABLE_URL_OVERRIDE = new GURL(UrlConstants.NTP_NON_NATIVE_URL);
    public static final String UNSAVEABLE_TAB_TITLE = "Unsavable tab";
    public static final GURL NTP_URL = new GURL(UrlConstants.NTP_NON_NATIVE_URL);
    public static final String NEW_TAB_TITLE = "New tab";

    // On startup, we look for any unsynced local tab groups and add them to sync. But if the group
    // was too old in past, we don't consider them relevant and exclude them from sync in order to
    // avoid noise in the tab group list surface.
    public static final int
            DEFAULT_MAX_DAYS_OF_STALENESS_ACCEPTED_FOR_ADDING_TAB_GROUP_TO_SYNC_ON_STARTUP =
                    Integer.MAX_VALUE;
    public static final String
            PARAM_MAX_DAYS_OF_STALENESS_ACCEPTED_FOR_ADDING_TAB_GROUP_TO_SYNC_ON_STARTUP =
                    "max_days_of_staleness_accepted_for_adding_tab_group_to_sync_on_startup";

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
        assert !filter.isIncognito();

        for (String syncGroupId : tabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(syncGroupId);
            // If there is no local ID the group is already hidden so this is a no-op.
            if (savedTabGroup.localId == null) continue;

            if (!isInCurrentWindow(filter, savedTabGroup.localId)) {
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
    public static SavedTabGroup getSavedTabGroupFromTabId(
            int tabId,
            @NonNull TabModel tabModel,
            @NonNull TabGroupSyncService tabGroupSyncService) {
        @Nullable Tab tab = tabModel.getTabById(tabId);
        if (tab == null || tab.getTabGroupId() == null) return null;
        LocalTabGroupId localTabGroupId = new LocalTabGroupId(tab.getTabGroupId());
        return tabGroupSyncService.getGroup(localTabGroupId);
    }

    /**
     * @return Whether a tab group is ineligible for syncing, e.g. too old to sync. An ineligible
     *     group will be skipped from adding to sync during initial sync on startup.
     */
    public static boolean isTabGroupEligibleForSyncing(
            LocalTabGroupId localTabGroupId, TabGroupModelFilter tabGroupModelFilter) {
        long lastAccessTime =
                getTabGroupLastAccessTime(localTabGroupId.tabGroupId, tabGroupModelFilter);
        long currentTime = TimeUtils.currentTimeMillis();
        int maxDaysOfStalenessAccepted =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
                        PARAM_MAX_DAYS_OF_STALENESS_ACCEPTED_FOR_ADDING_TAB_GROUP_TO_SYNC_ON_STARTUP,
                        DEFAULT_MAX_DAYS_OF_STALENESS_ACCEPTED_FOR_ADDING_TAB_GROUP_TO_SYNC_ON_STARTUP);
        long maxStalenessAcceptedInMillis =
                TimeUnit.MILLISECONDS.convert(maxDaysOfStalenessAccepted, TimeUnit.DAYS);
        return currentTime - lastAccessTime < maxStalenessAcceptedInMillis;
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
        int rootId = tabGroupModelFilter.getRootIdFromStableId(tabGroupId);
        List<Tab> tabs = tabGroupModelFilter.getRelatedTabListForRootId(rootId);
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
        if (tab.getTabGroupId() == null) return;
        TabGroupSyncUtilsJni.get()
                .onDidFinishNavigation(
                        tab.getProfile(),
                        getLocalTabGroupId(tab),
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
        if (tab.getTabGroupId() == null) return false;
        return TabGroupSyncUtilsJni.get()
                .isUrlInTabRedirectChain(
                        tab.getProfile(), getLocalTabGroupId(tab), tab.getId(), url);
    }

    @NativeMethods
    interface Natives {
        void onDidFinishNavigation(
                @JniType("Profile*") Profile profile,
                LocalTabGroupId groupId,
                int tabId,
                long navigationHandlePtr);

        boolean isUrlInTabRedirectChain(
                @JniType("Profile*") Profile profile,
                LocalTabGroupId groupId,
                int tabId,
                @JniType("GURL") GURL url);
    }
}
