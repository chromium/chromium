// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * For tab group lists to interact with {@link TabGroupSyncService} and multiple windows. Evaluates
 * tab group state relative to a specific current {@link TabModel} to determine whether groups
 * reside in the current window, another window, or are hidden/closing.
 */
@NullMarked
public class GroupWindowChecker {
    public static final Comparator<GroupWindowInfo> UPDATE_TIME_COMPARATOR =
            (a, b) -> Long.compare(b.lastModifiedTimeMs, a.lastModifiedTimeMs);

    /** Used to filter tab groups while processing tab groups. */
    @FunctionalInterface
    public interface TabGroupSelectionPredicate {
        /**
         * Whether a tab group should be included in a filtered list of tab groups.
         *
         * @param groupWindowState The {@link GroupWindowState} for the tab group.
         */
        boolean shouldInclude(@GroupWindowState Integer groupWindowState);
    }

    private final Context mContext;
    private final @Nullable TabGroupSyncService mSyncService;
    private final TabModel mCurrentTabModel;

    /**
     * @param context The {@link Context} for resources.
     * @param syncService The {@link TabGroupSyncService} for synced tab groups.
     * @param currentTabModel The active {@link TabModel} for the current window.
     */
    public GroupWindowChecker(
            Context context, @Nullable TabGroupSyncService syncService, TabModel currentTabModel) {
        mContext = context;
        mSyncService = syncService;
        mCurrentTabModel = currentTabModel;
    }

    /**
     * Returns a sorted list of {@link GroupWindowInfo}s using the default filter and comparator.
     */
    public List<GroupWindowInfo> getDefaultSortedGroupList() {
        return getSortedGroupList(
                GroupWindowChecker::shouldShowGroupByState, UPDATE_TIME_COMPARATOR);
    }

    /**
     * Returns a sorted list of {@link GroupWindowInfo}s.
     *
     * <p>The list includes all synced tab groups filtered by the provided predicate and sorted
     * using the provided comparator.
     *
     * @param tabGroupSelectionPredicate The predicate used for selecting tab groups which should be
     *     included in the returned list.
     * @param comparator Used for sorting the list.
     */
    public List<GroupWindowInfo> getSortedGroupList(
            TabGroupSelectionPredicate tabGroupSelectionPredicate,
            Comparator<GroupWindowInfo> comparator) {
        List<GroupWindowInfo> groupList = new ArrayList<>();
        if (mSyncService != null && !mCurrentTabModel.isIncognito()) {
            addSyncedTabGroups(groupList, tabGroupSelectionPredicate);
        } else {
            addLocalTabGroups(groupList, tabGroupSelectionPredicate);
        }
        groupList.sort(comparator);
        return groupList;
    }

    private void addSyncedTabGroups(
            List<GroupWindowInfo> groupList,
            TabGroupSelectionPredicate tabGroupSelectionPredicate) {
        assert mSyncService != null;
        for (String syncGroupId : mSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mSyncService.getGroup(syncGroupId);
            assert savedTabGroup != null && !savedTabGroup.savedTabs.isEmpty();

            @GroupWindowState int groupWindowState = getState(savedTabGroup);
            if (tabGroupSelectionPredicate.shouldInclude(groupWindowState)) {
                groupList.add(
                        GroupWindowInfo.forSyncedGroup(mContext, savedTabGroup, groupWindowState));
            }
        }
    }

    private void addLocalTabGroups(
            List<GroupWindowInfo> groupList,
            TabGroupSelectionPredicate tabGroupSelectionPredicate) {
        Set<Token> seenGroups = new HashSet<>();
        for (TabModel model : getAllTabModels()) {
            for (Token groupId : model.getAllTabGroupIds()) {
                if (!seenGroups.add(groupId)) {
                    continue;
                }
                @GroupWindowState int groupWindowState = getState(groupId);
                if (tabGroupSelectionPredicate.shouldInclude(groupWindowState)) {
                    groupList.add(
                            GroupWindowInfo.forLocalGroup(
                                    mContext, model, groupId, groupWindowState));
                }
            }
        }
    }

    /**
     * Returns whether there is any tab group other than the given group ID.
     *
     * @param currentGroupId The tab group ID to exclude, or null if checking for any tab group.
     * @return True if another tab group exists, false otherwise.
     */
    public boolean hasOtherGroups(@Nullable Token currentGroupId) {
        for (GroupWindowInfo group : getDefaultSortedGroupList()) {
            if (group.localId != null && !Objects.equals(currentGroupId, group.localId)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Whether a tab group should be shown based on its {@link GroupWindowState}.
     *
     * @param state The {@link GroupWindowState} of the group.
     */
    public static boolean shouldShowGroupByState(@GroupWindowState int state) {
        if (state == GroupWindowState.IN_CURRENT_CLOSING || state == GroupWindowState.HIDDEN) {
            return false;
        }
        if (state == GroupWindowState.IN_ANOTHER) {
            return TabGroupUiUtils.isCrossWindowTabGroupOperationsEnabled();
        }
        return true;
    }

    /**
     * Returns the {@link GroupWindowState} of the given {@link SavedTabGroup}.
     *
     * @param savedTabGroup The {@link SavedTabGroup} to check.
     * @return The {@link GroupWindowState} of the saved tab group.
     */
    public @GroupWindowState int getState(SavedTabGroup savedTabGroup) {
        if (savedTabGroup.localId == null) {
            return GroupWindowState.HIDDEN;
        }
        return getState(savedTabGroup.localId.tabGroupId);
    }

    /**
     * Returns the {@link GroupWindowState} of the given local tab group ID.
     *
     * @param groupId The local tab group ID {@link Token}.
     * @return The {@link GroupWindowState} of the tab group.
     */
    public @GroupWindowState int getState(Token groupId) {
        if (!containsGroup(groupId)) {
            if (TabGroupUiUtils.isCrossWindowTabGroupOperationsEnabled()
                    && isWindowForGroupNotActive(groupId)) {
                return GroupWindowState.HIDDEN;
            }
            return GroupWindowState.IN_ANOTHER;
        }

        return isGroupFullyClosing(groupId)
                ? GroupWindowState.IN_CURRENT_CLOSING
                : GroupWindowState.IN_CURRENT;
    }

    private boolean containsGroup(Token groupId) {
        TabList tabList = mCurrentTabModel.getComprehensiveModel();
        if (tabList != null) {
            for (Tab tab : tabList) {
                if (groupId.equals(tab.getTabGroupId())) {
                    return true;
                }
            }
            return false;
        }
        return mCurrentTabModel.tabGroupExists(groupId);
    }

    private boolean isGroupFullyClosing(Token groupId) {
        TabList tabList = mCurrentTabModel.getComprehensiveModel();
        if (tabList == null) {
            return false;
        }
        boolean isFullyClosing = true;
        for (Tab tab : tabList) {
            if (groupId.equals(tab.getTabGroupId())) {
                isFullyClosing &= tab.isClosing();
            }
        }
        return isFullyClosing;
    }

    private List<TabModel> getAllTabModels() {
        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        if (tabWindowManager == null || !TabGroupUiUtils.isCrossWindowTabGroupOperationsEnabled()) {
            return List.of(mCurrentTabModel);
        }

        Collection<TabModelSelector> selectors = tabWindowManager.getAllTabModelSelectors();
        if (selectors.isEmpty()) {
            return List.of(mCurrentTabModel);
        }

        List<TabModel> tabModels = new ArrayList<>();
        for (TabModelSelector selector : selectors) {
            tabModels.add(selector.getModel(mCurrentTabModel.isIncognito()));
        }
        return tabModels;
    }

    private boolean isWindowForGroupNotActive(Token groupId) {
        TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
        if (windowManager == null) {
            return false;
        }

        @WindowId int windowId = windowManager.findWindowIdForTabGroup(groupId);
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) {
            return false;
        }

        TabModelSelector selector = windowManager.getTabModelSelectorById(windowId);
        if (selector == null) {
            return false;
        }

        return selector.getModel(mCurrentTabModel.isIncognito()).getTabModelType()
                == TabModelType.HEADLESS;
    }
}
