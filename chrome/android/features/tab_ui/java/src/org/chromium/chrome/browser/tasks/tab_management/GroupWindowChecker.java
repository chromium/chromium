// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
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
    private final Map<Token, TabModel> mModelByGroupId = new HashMap<>();
    private final Map<Token, List<Tab>> mTabsByGroupId = new HashMap<>();
    private final Map<Token, List<Tab>> mClosingTabsByGroupId = new HashMap<>();
    private final Set<Token> mActiveGroupIds = new HashSet<>();
    private final Map<String, SavedTabGroup> mSavedGroupsById = new HashMap<>();

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
        invalidate();
    }

    /** Rebuilds the internal snapshot maps indexing tab groups across all available tab models. */
    public void invalidate() {
        mModelByGroupId.clear();
        mTabsByGroupId.clear();
        mClosingTabsByGroupId.clear();
        mActiveGroupIds.clear();
        mSavedGroupsById.clear();

        for (TabModel model : getAllTabModels()) {
            indexTabModel(model);
        }
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
        Set<Token> seenGroups = new HashSet<>();
        if (mSyncService != null && !mCurrentTabModel.isIncognito()) {
            addSyncedTabGroups(groupList, tabGroupSelectionPredicate, seenGroups);
        }
        addLocalTabGroups(groupList, tabGroupSelectionPredicate, seenGroups);
        groupList.sort(comparator);
        return groupList;
    }

    private void addSyncedTabGroups(
            List<GroupWindowInfo> groupList,
            TabGroupSelectionPredicate tabGroupSelectionPredicate,
            Set<Token> seenGroups) {
        assert mSyncService != null;
        for (String syncGroupId : mSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = getSavedTabGroup(syncGroupId);
            assert savedTabGroup != null && !savedTabGroup.savedTabs.isEmpty();

            if (savedTabGroup.localId != null) {
                seenGroups.add(savedTabGroup.localId.tabGroupId);
            }

            @GroupWindowState int groupWindowState = getState(savedTabGroup);
            if (tabGroupSelectionPredicate.shouldInclude(groupWindowState)) {
                groupList.add(fromSavedTabGroup(savedTabGroup));
            }
        }
    }

    private TabModel getTabModelForGroup(SavedTabGroup savedTabGroup) {
        if (!TabGroupUiUtils.isRemoteGroupOperationsEnabled() || savedTabGroup.localId == null) {
            return mCurrentTabModel;
        }
        TabModel model = mModelByGroupId.get(savedTabGroup.localId.tabGroupId);
        if (model != null) {
            return model;
        }
        return TabGroupUiUtils.getTabModelForGroup(
                mCurrentTabModel, savedTabGroup.localId.tabGroupId);
    }

    private void addLocalTabGroups(
            List<GroupWindowInfo> groupList,
            TabGroupSelectionPredicate tabGroupSelectionPredicate,
            Set<Token> seenGroups) {
        for (TabModel model : getAllTabModels()) {
            for (Token groupId : model.getAllTabGroupIds()) {
                if (!seenGroups.add(groupId)) {
                    continue;
                }
                @GroupWindowState int groupWindowState = getState(groupId);
                if (groupWindowState == GroupWindowState.IN_CURRENT_CLOSING) {
                    continue;
                }
                if (tabGroupSelectionPredicate.shouldInclude(groupWindowState)) {
                    groupList.add(
                            GroupWindowInfo.forLocalGroup(
                                    mContext, model, groupId, groupWindowState, getTabs(groupId)));
                }
            }
        }
    }

    /**
     * Returns a {@link GroupWindowInfo} for the given {@link SavedTabGroup}.
     *
     * @param savedTabGroup The saved tab group to convert.
     */
    public GroupWindowInfo fromSavedTabGroup(SavedTabGroup savedTabGroup) {
        @GroupWindowState int state = getState(savedTabGroup);
        TabModel targetModel = getTabModelForGroup(savedTabGroup);
        Token localId = savedTabGroup.localId != null ? savedTabGroup.localId.tabGroupId : null;
        boolean hasMatchingModel = localId != null && mModelByGroupId.get(localId) == targetModel;
        List<Tab> localTabs = hasMatchingModel ? getTabs(localId) : Collections.emptyList();
        return GroupWindowInfo.forSyncedGroup(
                mContext, targetModel, savedTabGroup, state, localTabs);
    }

    /**
     * Returns whether there is any tab group other than the given group ID that is a valid
     * destination for tab movement.
     *
     * @param currentGroupId The tab group ID to exclude, or null if checking for any tab group.
     * @return True if another valid destination tab group exists, false otherwise.
     */
    public boolean hasOtherGroups(@Nullable Token currentGroupId) {
        Set<Token> seenGroups = new HashSet<>();
        if (hasOtherSyncedGroup(currentGroupId, seenGroups)) {
            return true;
        }
        return hasOtherLocalGroup(currentGroupId, seenGroups);
    }

    private boolean hasOtherSyncedGroup(@Nullable Token currentGroupId, Set<Token> seenGroups) {
        if (mSyncService == null || mCurrentTabModel.isIncognito()) {
            return false;
        }

        for (String syncGroupId : mSyncService.getAllGroupIds()) {
            if (isValidSyncedDestination(
                    getSavedTabGroup(syncGroupId), currentGroupId, seenGroups)) {
                return true;
            }
        }
        return false;
    }

    private @Nullable SavedTabGroup getSavedTabGroup(String syncGroupId) {
        if (mSyncService == null) {
            return null;
        }
        return mSavedGroupsById.computeIfAbsent(syncGroupId, mSyncService::getGroup);
    }

    private boolean isValidSyncedDestination(
            @Nullable SavedTabGroup savedTabGroup,
            @Nullable Token currentGroupId,
            Set<Token> seenGroups) {
        if (savedTabGroup == null) {
            return false;
        }
        Token localGroupId =
                savedTabGroup.localId != null ? savedTabGroup.localId.tabGroupId : null;
        if (localGroupId != null) {
            seenGroups.add(localGroupId);
            if (localGroupId.equals(currentGroupId)) {
                return false;
            }
        }
        if (savedTabGroup.savedTabs == null || savedTabGroup.savedTabs.isEmpty()) {
            return false;
        }
        @GroupWindowState int state = getState(savedTabGroup);
        return isValidGroupDestination(state, savedTabGroup.syncId);
    }

    private boolean hasOtherLocalGroup(@Nullable Token currentGroupId, Set<Token> seenGroups) {
        for (Token groupId : mModelByGroupId.keySet()) {
            if (isValidLocalDestination(groupId, currentGroupId, seenGroups)) {
                return true;
            }
        }
        return false;
    }

    private boolean isValidLocalDestination(
            Token groupId, @Nullable Token currentGroupId, Set<Token> seenGroups) {
        if (!seenGroups.add(groupId) || groupId.equals(currentGroupId)) {
            return false;
        }

        @GroupWindowState int state = getState(groupId);
        return isValidGroupDestination(state, /* syncId= */ null);
    }

    private static boolean isValidGroupDestination(
            @GroupWindowState int state, @Nullable String syncId) {
        if (!shouldShowGroupByState(state)) {
            return false;
        }
        if (state == GroupWindowState.HIDDEN || state == GroupWindowState.IN_CURRENT_CLOSING) {
            return !TextUtils.isEmpty(syncId);
        }
        return true;
    }

    /**
     * Whether a tab group should be shown based on its {@link GroupWindowState}.
     *
     * @param state The {@link GroupWindowState} of the group.
     */
    public static boolean shouldShowGroupByState(@GroupWindowState int state) {
        if (state == GroupWindowState.IN_CURRENT_CLOSING || state == GroupWindowState.HIDDEN) {
            return TabGroupUiUtils.isRemoteGroupOperationsEnabled();
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
            if (isGroupClosingInAnotherWindow(groupId)) {
                return GroupWindowState.HIDDEN;
            }
            return GroupWindowState.IN_ANOTHER;
        }

        return isGroupFullyClosing(groupId)
                ? GroupWindowState.IN_CURRENT_CLOSING
                : GroupWindowState.IN_CURRENT;
    }

    private void indexTabModel(TabModel model) {
        indexComprehensiveModel(model);
        for (Token groupId : model.getAllTabGroupIds()) {
            indexGroupId(model, groupId);
        }
    }

    private void indexComprehensiveModel(TabModel model) {
        TabList comprehensiveModel = model.getComprehensiveModel();
        if (comprehensiveModel == null) {
            return;
        }
        if (comprehensiveModel.iterator() != null) {
            for (Tab tab : comprehensiveModel) {
                indexTab(model, tab);
            }
            return;
        }
        int count = comprehensiveModel.getCount();
        for (int i = 0; i < count; i++) {
            Tab tab = comprehensiveModel.getTabAt(i);
            indexTab(model, tab);
        }
    }

    private void indexTab(TabModel model, @Nullable Tab tab) {
        if (tab == null) {
            return;
        }
        Token groupId = tab.getTabGroupId();
        if (groupId == null) {
            return;
        }
        TabModel existingModel = mModelByGroupId.putIfAbsent(groupId, model);
        if (existingModel != null && existingModel != model) {
            return;
        }
        if (tab.isClosing()) {
            mClosingTabsByGroupId.computeIfAbsent(groupId, k -> new ArrayList<>()).add(tab);
            return;
        }
        mTabsByGroupId.computeIfAbsent(groupId, k -> new ArrayList<>()).add(tab);
        mActiveGroupIds.add(groupId);
    }

    private void indexGroupId(TabModel model, @Nullable Token groupId) {
        if (groupId == null) {
            return;
        }
        TabModel existingModel = mModelByGroupId.putIfAbsent(groupId, model);
        if (existingModel != null && existingModel != model) {
            return;
        }
        if (!mClosingTabsByGroupId.containsKey(groupId)) {
            mActiveGroupIds.add(groupId);
        }
    }

    private List<Tab> getTabs(@Nullable Token groupId) {
        if (groupId == null) {
            return Collections.emptyList();
        }
        List<Tab> activeTabs = mTabsByGroupId.get(groupId);
        if (activeTabs != null && !activeTabs.isEmpty()) {
            return activeTabs;
        }
        return mClosingTabsByGroupId.getOrDefault(groupId, Collections.emptyList());
    }

    private boolean containsGroup(Token groupId) {
        if (mModelByGroupId.containsKey(groupId)) {
            return mModelByGroupId.get(groupId) == mCurrentTabModel;
        }
        return mCurrentTabModel.containsTabGroup(groupId);
    }

    private boolean isGroupFullyClosing(Token groupId) {
        if (mModelByGroupId.containsKey(groupId)) {
            return !mActiveGroupIds.contains(groupId);
        }
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
        tabModels.add(mCurrentTabModel);
        for (TabModelSelector selector : selectors) {
            TabModel model = selector.getModel(mCurrentTabModel.isIncognito());
            if (model != mCurrentTabModel) {
                tabModels.add(model);
            }
        }
        return tabModels;
    }

    private boolean isGroupClosingInAnotherWindow(Token groupId) {
        TabModel otherModel = mModelByGroupId.get(groupId);
        if (otherModel != null && otherModel != mCurrentTabModel) {
            return !mActiveGroupIds.contains(groupId);
        }

        TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
        if (windowManager == null) {
            return false;
        }
        @WindowId
        int windowId =
                windowManager.findWindowIdForTabGroup(groupId, /* includeClosingGroups= */ true);
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) {
            return false;
        }
        TabModelSelector selector = windowManager.getTabModelSelectorById(windowId);
        if (selector == null) {
            return false;
        }
        TabModel fallbackOtherModel = selector.getModel(mCurrentTabModel.isIncognito());
        return !fallbackOtherModel.containsTabGroup(groupId);
    }
}
