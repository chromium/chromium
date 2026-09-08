// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;

/** Encapsulates metadata, window location, and sync status for a tab group. */
@NullMarked
public class GroupWindowInfo {
    public final @Nullable Token localId;
    public final @Nullable String syncId;
    public final String title;
    public final @TabGroupColorId int color;
    public final int tabCount;
    public final List<GURL> faviconUrls;
    public final @GroupWindowState int groupWindowState;
    public final long lastModifiedTimeMs;

    /**
     * Constructs a new {@link GroupWindowInfo} instance.
     *
     * @param localId The local tab group ID token, if available.
     * @param syncId The sync ID string, if available.
     * @param title The displayable title of the tab group.
     * @param color The color ID of the tab group.
     * @param tabCount The number of tabs in the tab group.
     * @param faviconUrls The list of URLs for rendering the favicon cluster.
     * @param groupWindowState The {@link GroupWindowState} location of the tab group.
     * @param lastModifiedTimeMs The last modified timestamp in milliseconds.
     */
    public GroupWindowInfo(
            @Nullable Token localId,
            @Nullable String syncId,
            String title,
            @TabGroupColorId int color,
            int tabCount,
            List<GURL> faviconUrls,
            @GroupWindowState int groupWindowState,
            long lastModifiedTimeMs) {
        this.localId = localId;
        this.syncId = syncId;
        this.title = title;
        this.color = color;
        this.tabCount = tabCount;
        this.faviconUrls = faviconUrls;
        this.groupWindowState = groupWindowState;
        this.lastModifiedTimeMs = lastModifiedTimeMs;
    }

    /**
     * Factory method for creating {@link GroupWindowInfo} for a synced tab group.
     *
     * @param context Context for resolving fallback titles.
     * @param savedGroup The {@link SavedTabGroup} instance.
     * @param state The {@link GroupWindowState} of the group.
     * @return A new {@link GroupWindowInfo} instance.
     */
    public static GroupWindowInfo forSyncedGroup(
            Context context, SavedTabGroup savedGroup, @GroupWindowState int state) {
        Token localId = savedGroup.localId != null ? savedGroup.localId.tabGroupId : null;
        int tabCount = savedGroup.savedTabs != null ? savedGroup.savedTabs.size() : 0;
        String title =
                TextUtils.isEmpty(savedGroup.title)
                        ? TabGroupTitleUtils.getDefaultTitle(context, tabCount)
                        : savedGroup.title;
        List<GURL> faviconUrls =
                savedGroup.savedTabs != null
                        ? TabGroupFaviconCluster.buildUrlListFromSyncGroup(savedGroup)
                        : Collections.emptyList();
        long lastModifiedTimeMs = savedGroup.updateTimeMs;
        if (savedGroup.savedTabs != null) {
            for (SavedTabGroupTab tab : savedGroup.savedTabs) {
                lastModifiedTimeMs = Math.max(lastModifiedTimeMs, tab.updateTimeMs);
            }
        }
        return new GroupWindowInfo(
                localId,
                savedGroup.syncId,
                title,
                savedGroup.color,
                tabCount,
                faviconUrls,
                state,
                lastModifiedTimeMs);
    }

    /**
     * Factory method for creating {@link GroupWindowInfo} for a local tab group.
     *
     * @param context Context for resolving default titles.
     * @param tabModel The {@link TabModel} containing the tab group.
     * @param groupId The local tab group ID {@link Token}.
     * @param state The {@link GroupWindowState} of the group.
     * @return A new {@link GroupWindowInfo} instance.
     */
    public static GroupWindowInfo forLocalGroup(
            Context context, TabModel tabModel, Token groupId, @GroupWindowState int state) {
        int tabCount = tabModel.getTabCountForGroup(groupId);
        String title = TabGroupTitleUtils.getDisplayableTitle(context, tabModel, groupId);
        @TabGroupColorId int color = tabModel.getTabGroupColorWithFallback(groupId);
        List<GURL> faviconUrls = TabGroupFaviconCluster.buildUrlListFromFilter(groupId, tabModel);
        long lastModifiedTimeMs = getLastModifiedTimeMs(tabModel.getTabsInGroup(groupId));

        return new GroupWindowInfo(
                groupId,
                /* syncId= */ null,
                title,
                color,
                tabCount,
                faviconUrls,
                state,
                lastModifiedTimeMs);
    }

    private static long getLastModifiedTimeMs(List<Tab> tabs) {
        long lastModifiedTimeMs = 0L;
        for (Tab tab : tabs) {
            lastModifiedTimeMs = Math.max(lastModifiedTimeMs, tab.getTimestampMillis());
        }
        return lastModifiedTimeMs;
    }
}
