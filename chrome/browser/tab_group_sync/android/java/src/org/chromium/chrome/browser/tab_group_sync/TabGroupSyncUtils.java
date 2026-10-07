// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNtpGurl;

import android.util.Pair;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;

/** Utility methods for tab group sync. */
@NullMarked
public final class TabGroupSyncUtils {
    // The URL written to sync when the local URL isn't in a syncable format, i.e. HTTP or HTTPS.
    public static final GURL UNSAVEABLE_URL_OVERRIDE = getOriginalNtpGurl();
    public static final String UNSAVEABLE_TAB_TITLE = "Unsavable tab";
    public static final GURL NTP_URL = getOriginalNtpGurl();
    public static final String NEW_TAB_TITLE = "New tab";

    /**
     * Whether the given {@code localId} corresponds to a tab group in the current window
     * corresponding to {@code tabModel}.
     *
     * @param tabModel The tab model in which to find the tab group.
     * @param localId The ID of the tab group.
     */
    public static boolean isInCurrentWindow(TabModel tabModel, LocalTabGroupId localId) {
        return tabModel.containsTabGroup(localId.tabGroupId);
    }

    private static boolean isInAnyWindow(LocalTabGroupId localId, List<TabModel> tabModelList) {
        for (TabModel tabModel : tabModelList) {
            if (isInCurrentWindow(tabModel, localId)) {
                return true;
            }
        }
        return false;
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
        } else if (UrlUtilities.isNtpOrAboutBlank(url)) {
            return new Pair<>(NTP_URL, NEW_TAB_TITLE);
        } else {
            return new Pair<>(UNSAVEABLE_URL_OVERRIDE, UNSAVEABLE_TAB_TITLE);
        }
    }

    /** Utility method to determine if a URL can be synced or not. */
    public static boolean isSavableUrl(GURL url) {
        return UrlUtilities.isHttpOrHttps(url);
    }

    /**
     * Removes all tab groups mappings found in the {@link TabGroupSyncService} that don't have
     * corresponding local IDs in the {@link TabModel}.
     *
     * @param tabGroupSyncService The {@link TabGroupSyncService} to remove tabs from.
     * @param tabModel The {@link TabModel} to check for tab groups.
     */
    public static void unmapLocalIdsNotInTabModel(
            TabGroupSyncService tabGroupSyncService, TabModel tabModel) {
        unmapLocalIdsNotInTabModelList(tabGroupSyncService, Collections.singletonList(tabModel));
    }

    /** Same as {@link #unmapLocalIdsNotInTabModel} only with a list of tab models. */
    public static void unmapLocalIdsNotInTabModelList(
            TabGroupSyncService tabGroupSyncService, List<TabModel> tabModelList) {
        for (TabModel tabModel : tabModelList) {
            assert !tabModel.isOffTheRecord();
        }

        for (String syncGroupId : tabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(syncGroupId);
            // If there is no local ID the group is already hidden so this is a no-op.
            if (savedTabGroup == null || savedTabGroup.localId == null) continue;

            if (!isInAnyWindow(savedTabGroup.localId, tabModelList)) {
                tabGroupSyncService.removeLocalTabGroupMapping(
                        savedTabGroup.localId, ClosingSource.CLEANED_UP_ON_LAST_INSTANCE_CLOSURE);
            }
        }
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
