// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

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
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;

/** For tab group lists to interact with {@link TabGroupSyncService} and multiple windows. */
@NullMarked
public class GroupWindowChecker {
    public static final Comparator<SavedTabGroup> UPDATE_TIME_COMPARATOR =
            (a, b) -> Long.compare(b.updateTimeMs, a.updateTimeMs);

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

    private final @Nullable TabGroupSyncService mSyncService;
    private final TabModel mTabModel;

    /**
     * @param syncService The service to use for accessing synced tab groups.
     * @param tabModel Used for accessing tab information.
     */
    public GroupWindowChecker(@Nullable TabGroupSyncService syncService, TabModel tabModel) {
        mSyncService = syncService;
        mTabModel = tabModel;
    }

    /** Returns a sorted list of {@link SavedTabGroup}s using the default filter and comparator. */
    public List<SavedTabGroup> getDefaultSortedGroupList() {
        return getSortedGroupList(
                GroupWindowChecker::shouldShowGroupByState, UPDATE_TIME_COMPARATOR);
    }

    /**
     * Returns a sorted list of {@link SavedTabGroup}s.
     *
     * <p>The list includes all synced tab groups filtered by the provided predicate and sorted
     * using the provided comparator.
     *
     * @param tabGroupSelectionPredicate The predicate used for selecting tab groups which should be
     *     included in the returned list.
     * @param comparator Used for sorting the list.
     */
    public List<SavedTabGroup> getSortedGroupList(
            TabGroupSelectionPredicate tabGroupSelectionPredicate,
            Comparator<SavedTabGroup> comparator) {
        List<SavedTabGroup> groupList = new ArrayList<>();
        if (mSyncService == null) return groupList;

        for (String syncGroupId : mSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mSyncService.getGroup(syncGroupId);
            assert savedTabGroup != null && !savedTabGroup.savedTabs.isEmpty();

            @GroupWindowState int groupWindowState = getState(savedTabGroup);
            if (tabGroupSelectionPredicate.shouldInclude(groupWindowState)) {
                groupList.add(savedTabGroup);
            }
        }
        groupList.sort(comparator);
        return groupList;
    }

    /**
     * Returns whether there is any tab group other than the given group ID.
     *
     * @param currentGroupId The tab group ID to exclude, or null if checking for any tab group.
     * @return True if another tab group exists, false otherwise.
     */
    public boolean hasOtherGroups(@Nullable Token currentGroupId) {
        for (SavedTabGroup group : getDefaultSortedGroupList()) {
            if (group.localId != null
                    && !Objects.equals(currentGroupId, group.localId.tabGroupId)) {
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

    /** Returns the {@link GroupWindowState} of the given {@link SavedTabGroup}. */
    public @GroupWindowState int getState(SavedTabGroup savedTabGroup) {
        if (savedTabGroup.localId == null) {
            return GroupWindowState.HIDDEN;
        }

        Token groupId = savedTabGroup.localId.tabGroupId;
        boolean isFullyClosing = true;
        boolean foundGroup = false;

        TabList tabList = mTabModel.getComprehensiveModel();
        for (Tab tab : tabList) {
            if (groupId.equals(tab.getTabGroupId())) {
                foundGroup = true;
                isFullyClosing &= tab.isClosing();
            }
        }
        if (!foundGroup) {
            if (TabGroupUiUtils.isCrossWindowTabGroupOperationsEnabled()
                    && isWindowForGroupNotActive(groupId)) {
                return GroupWindowState.HIDDEN;
            }
            return GroupWindowState.IN_ANOTHER;
        }

        // If the group is only partially closing no special case is required since we still have to
        // do all the IN_CURRENT work and returning to the tab group via the dialog will work.
        return isFullyClosing ? GroupWindowState.IN_CURRENT_CLOSING : GroupWindowState.IN_CURRENT;
    }

    private boolean isWindowForGroupNotActive(Token groupId) {
        TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
        if (windowManager == null) {
            return false;
        }

        int windowId = windowManager.findWindowIdForTabGroup(groupId);
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) {
            return false;
        }

        TabModelSelector selector = windowManager.getTabModelSelectorById(windowId);
        if (selector == null) {
            return false;
        }

        return selector.getModel(/* incognito= */ false).getTabModelType() == TabModelType.HEADLESS;
    }
}
