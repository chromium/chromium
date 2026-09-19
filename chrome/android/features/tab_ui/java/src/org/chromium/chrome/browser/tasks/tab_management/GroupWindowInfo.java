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
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

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
     * @param title The display title of the group.
     * @param color The color of the tab group.
     * @param tabCount The number of tabs in the group.
     * @param faviconUrls The favicon URLs for tabs in the group.
     * @param groupWindowState The {@link GroupWindowState} of the group.
     * @param lastModifiedTimeMs The last modified time of the group in milliseconds.
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
     * Factory method for creating {@link GroupWindowInfo} for a synced tab group, prioritizing live
     * local {@link TabModel} metadata when the group is present locally.
     *
     * @param context Context for resolving fallback titles.
     * @param localTabModel The local {@link TabModel}.
     * @param savedGroup The {@link SavedTabGroup} instance.
     * @param state The {@link GroupWindowState} of the group.
     * @return A new {@link GroupWindowInfo} instance.
     */
    public static GroupWindowInfo forSyncedGroup(
            Context context,
            TabModel localTabModel,
            SavedTabGroup savedGroup,
            @GroupWindowState int state) {
        Token localId = savedGroup.localId != null ? savedGroup.localId.tabGroupId : null;
        return forLocalOrSyncedGroupInternal(context, localTabModel, localId, savedGroup, state);
    }

    /**
     * Factory method for creating {@link GroupWindowInfo} for a local tab group.
     *
     * @param context Context for resolving fallback titles.
     * @param tabModel The tab model containing the group.
     * @param groupId The target tab group ID.
     * @param state The {@link GroupWindowState} of the group.
     * @return A new {@link GroupWindowInfo} instance.
     */
    public static GroupWindowInfo forLocalGroup(
            Context context, TabModel tabModel, Token groupId, @GroupWindowState int state) {
        return forLocalOrSyncedGroupInternal(
                context, tabModel, groupId, /* savedGroup= */ null, state);
    }

    /**
     * Internal factory helper constructing a {@link GroupWindowInfo} from local {@link TabModel}
     * data, augmenting with optional sync metadata (syncId, fallback title, update time).
     *
     * @param context Context for resolving fallback titles.
     * @param localTabModel The local {@link TabModel}.
     * @param localGroupId The local tab group ID {@link Token}, if available.
     * @param savedGroup The {@link SavedTabGroup} instance, if synced.
     * @param state The {@link GroupWindowState} of the group.
     * @return A new {@link GroupWindowInfo} instance.
     */
    private static GroupWindowInfo forLocalOrSyncedGroupInternal(
            Context context,
            TabModel localTabModel,
            @Nullable Token localGroupId,
            @Nullable SavedTabGroup savedGroup,
            @GroupWindowState int state) {
        List<Tab> localTabs =
                localGroupId != null
                        ? getLocalTabsInGroup(localTabModel, localGroupId)
                        : Collections.emptyList();
        boolean hasLocalTabs = !localTabs.isEmpty();
        boolean hasLocalData =
                localTabModel != null
                        && localGroupId != null
                        && (hasLocalTabs || localTabModel.tabGroupExists(localGroupId));

        int tabCount = resolveTabCount(localTabModel, localGroupId, localTabs, savedGroup);
        String title =
                resolveTitle(
                        context, localTabModel, localGroupId, savedGroup, hasLocalData, tabCount);
        @TabGroupColorId
        int color = resolveColor(localTabModel, localGroupId, savedGroup, hasLocalData);
        List<GURL> faviconUrls =
                resolveFaviconUrls(localTabModel, localGroupId, localTabs, savedGroup);
        long lastModifiedTimeMs =
                resolveLastModifiedTimeMs(localTabModel, localGroupId, localTabs, savedGroup);
        String syncId = savedGroup != null ? savedGroup.syncId : null;

        return new GroupWindowInfo(
                localGroupId,
                syncId,
                title,
                color,
                tabCount,
                faviconUrls,
                state,
                lastModifiedTimeMs);
    }

    private static int resolveTabCount(
            @Nullable TabModel localTabModel,
            @Nullable Token localGroupId,
            List<Tab> localTabs,
            @Nullable SavedTabGroup savedGroup) {
        if (!localTabs.isEmpty()) {
            return localTabs.size();
        }
        if (localTabModel != null
                && localGroupId != null
                && localTabModel.tabGroupExists(localGroupId)) {
            int count = localTabModel.getTabCountForGroup(localGroupId);
            if (count > 0) {
                return count;
            }
        }
        return savedGroup != null && savedGroup.savedTabs != null ? savedGroup.savedTabs.size() : 0;
    }

    private static String resolveTitle(
            Context context,
            @Nullable TabModel localTabModel,
            @Nullable Token localGroupId,
            @Nullable SavedTabGroup savedGroup,
            boolean hasLocalData,
            int tabCount) {
        if (savedGroup == null && localTabModel != null && localGroupId != null) {
            return TabGroupTitleUtils.getDisplayableTitle(context, localTabModel, localGroupId);
        }
        String title = null;
        if (hasLocalData && localTabModel != null && localGroupId != null) {
            title = localTabModel.getTabGroupTitle(localGroupId);
        }
        if (TextUtils.isEmpty(title) && savedGroup != null) {
            title = savedGroup.title;
        }
        if (TextUtils.isEmpty(title)) {
            title = TabGroupTitleUtils.getDefaultTitle(context, tabCount);
        }
        return title;
    }

    private static @TabGroupColorId int resolveColor(
            @Nullable TabModel localTabModel,
            @Nullable Token localGroupId,
            @Nullable SavedTabGroup savedGroup,
            boolean hasLocalData) {
        if (hasLocalData && localTabModel != null && localGroupId != null) {
            return localTabModel.getTabGroupColorWithFallback(localGroupId);
        }
        return savedGroup != null ? savedGroup.color : TabGroupColorId.GREY;
    }

    private static List<GURL> resolveFaviconUrls(
            @Nullable TabModel localTabModel,
            @Nullable Token localGroupId,
            List<Tab> localTabs,
            @Nullable SavedTabGroup savedGroup) {
        List<GURL> faviconUrls = new ArrayList<>();
        if (!localTabs.isEmpty()) {
            int urlCount = Math.min(TabGroupFaviconCluster.CORNER_COUNT, localTabs.size());
            for (int i = 0; i < urlCount; i++) {
                faviconUrls.add(localTabs.get(i).getUrl());
            }
        } else if (savedGroup == null && localTabModel != null && localGroupId != null) {
            faviconUrls =
                    TabGroupFaviconCluster.buildUrlListFromFilter(localGroupId, localTabModel);
        } else if (savedGroup != null && savedGroup.savedTabs != null) {
            faviconUrls = TabGroupFaviconCluster.buildUrlListFromSyncGroup(savedGroup);
        }
        return faviconUrls;
    }

    private static long resolveLastModifiedTimeMs(
            @Nullable TabModel localTabModel,
            @Nullable Token localGroupId,
            List<Tab> localTabs,
            @Nullable SavedTabGroup savedGroup) {
        long localLastModified = 0L;
        if (!localTabs.isEmpty()) {
            localLastModified = getLastModifiedTimeMs(localTabs);
        } else if (localTabModel != null && localGroupId != null) {
            localLastModified = getLastModifiedTimeMs(localTabModel.getTabsInGroup(localGroupId));
        }
        long savedLastModified = savedGroup != null ? savedGroup.updateTimeMs : 0L;
        if (savedGroup != null && savedGroup.savedTabs != null) {
            for (SavedTabGroupTab tab : savedGroup.savedTabs) {
                savedLastModified = Math.max(savedLastModified, tab.updateTimeMs);
            }
        }
        return Math.max(localLastModified, savedLastModified);
    }

    /**
     * Returns the list of local {@link Tab}s belonging to {@code groupId}. If the group is
     * detached/pending closure (so {@link TabModel#getTabsInGroup(Token)} returns empty), falls
     * back to filtering {@link TabModel#getComprehensiveModel()}.
     *
     * @param tabModel The tab model containing tabs.
     * @param groupId The target tab group ID.
     * @return List of tabs belonging to the group.
     */
    private static List<Tab> getLocalTabsInGroup(
            @Nullable TabModel tabModel, @Nullable Token groupId) {
        if (tabModel == null || groupId == null) {
            return Collections.emptyList();
        }
        List<Tab> tabs = tabModel.getTabsInGroup(groupId);
        if (tabs != null && !tabs.isEmpty()) {
            return tabs;
        }
        if (!TabGroupUiUtils.isRemoteGroupOperationsEnabled()) {
            return Collections.emptyList();
        }
        List<Tab> allGroupTabs = new ArrayList<>();
        TabList comprehensiveModel = tabModel.getComprehensiveModel();
        if (comprehensiveModel != null) {
            if (comprehensiveModel.iterator() != null) {
                for (Tab tab : comprehensiveModel) {
                    if (Objects.equals(tab.getTabGroupId(), groupId)) {
                        allGroupTabs.add(tab);
                    }
                }
            } else {
                for (int i = 0; i < comprehensiveModel.getCount(); i++) {
                    Tab tab = comprehensiveModel.getTabAt(i);
                    if (tab != null && Objects.equals(tab.getTabGroupId(), groupId)) {
                        allGroupTabs.add(tab);
                    }
                }
            }
        }
        return allGroupTabs;
    }

    private static long getLastModifiedTimeMs(List<Tab> tabs) {
        long lastModifiedTimeMs = 0L;
        for (Tab tab : tabs) {
            lastModifiedTimeMs = Math.max(lastModifiedTimeMs, tab.getTimestampMillis());
        }
        return lastModifiedTimeMs;
    }
}
