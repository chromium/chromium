// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * An implementation of {@link TabModelFilter} that puts {@link Tab}s into a group
 * structure.
 *
 * A group is a collection of {@link Tab}s that share a common ancestor {@link Tab}. This filter is
 * also a {@link TabList} that contains the last shown {@link Tab} from every group.
 */
public class TabGroupModelFilter extends TabModelFilter {
    private static final String PREFS_FILE = "tab_group_pref";
    private static final String SESSIONS_COUNT_FOR_GROUP = "SessionsCountForGroup-";
    private static SharedPreferences sPref;

    /** An interface to be notified about changes to a {@link TabGroupModelFilter}. */
    public interface Observer {
        /**
         * This method is called before a tab is moved to form a group or moved into an existed
         * group.
         * @param movedTab The {@link Tab} which will be moved. If a group will be merged to a tab
         *                 or another group, this is the last tab of the merged group.
         * @param newRootId  The new root id of the group after merge.
         */
        void willMergeTabToGroup(Tab movedTab, int newRootId);

        /**
         * This method is called before a tab within a group is moved out of the group.
         *
         * @param movedTab   The tab which will be moved.
         * @param newRootId  The new root id of the group from which {@code movedTab} is moved out.
         */
        void willMoveTabOutOfGroup(Tab movedTab, int newRootId);

        /**
         * This method is called after a tab is moved to form a group or moved into an existed
         * group.
         * @param movedTab The {@link Tab} which has been moved. If a group is merged to a tab or
         *                 another group, this is the last tab of the merged group.
         * @param selectedTabIdInGroup The id of the selected {@link Tab} in group.
         */
        void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup);

        /**
         * This method is called after a group is moved.
         *
         * @param movedTab The tab which has been moved. This is the last tab within the group.
         * @param tabModelOldIndex The old index of the {@code movedTab} in the {@link TabModel}.
         * @param tabModelNewIndex The new index of the {@code movedTab} in the {@link TabModel}.
         */
        void didMoveTabGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex);

        /**
         * This method is called after a tab within a group is moved.
         *
         * @param movedTab The tab which has been moved.
         * @param tabModelOldIndex The old index of the {@code movedTab} in the {@link TabModel}.
         * @param tabModelNewIndex The new index of the {@code movedTab} in the {@link TabModel}.
         */
        void didMoveWithinGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex);

        /**
         * This method is called after a tab within a group is moved out of the group.
         *
         * @param movedTab The tab which has been moved.
         * @param prevFilterIndex The index in {@link TabGroupModelFilter} of the group where {@code
         *         moveTab} is in  before ungrouping.
         */
        void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex);

        /**
         * This method is called after a group is created manually by user. Either using the
         * TabListEditor (Group tab menu item) or using drag and drop.
         *
         * @param tabs The list of modified {@link Tab}s.
         * @param tabOriginalIndex The original tab index for each modified tab.
         * @param tabOriginalRootId The original root id for each modified tab.
         * @param destinationGroupTitle The original destination group title.
         */
        void didCreateGroup(
                List<Tab> tabs,
                List<Integer> tabOriginalIndex,
                List<Integer> tabOriginalRootId,
                String destinationGroupTitle);
    }

    /**
     * This class is a representation of a group of tabs. It knows the last selected tab within the
     * group.
     */
    private class TabGroup {
        private static final int INVALID_GROUP_ID = -1;
        private final Set<Integer> mTabIds;
        private int mLastShownTabId;
        private int mGroupId;

        TabGroup(int groupId) {
            mTabIds = new LinkedHashSet<>();
            mLastShownTabId = Tab.INVALID_TAB_ID;
            mGroupId = groupId;
        }

        void addTab(int tabId) {
            mTabIds.add(tabId);
            if (mLastShownTabId == Tab.INVALID_TAB_ID) setLastShownTabId(tabId);
            if (size() > 1) reorderGroup(mGroupId);
        }

        void removeTab(int tabId) {
            assert mTabIds.contains(tabId);
            if (mLastShownTabId == tabId) {
                int nextIdToShow = nextTabIdToShow(tabId);
                if (nextIdToShow != Tab.INVALID_TAB_ID) setLastShownTabId(nextIdToShow);
            }
            mTabIds.remove(tabId);
        }

        void moveToEndInGroup(int tabId) {
            if (!mTabIds.contains(tabId)) return;
            mTabIds.remove(tabId);
            mTabIds.add(tabId);
        }

        boolean contains(int tabId) {
            return mTabIds.contains(tabId);
        }

        int size() {
            return mTabIds.size();
        }

        List<Integer> getTabIdList() {
            return Collections.unmodifiableList(new ArrayList<>(mTabIds));
        }

        int getLastShownTabId() {
            return mLastShownTabId;
        }

        void setLastShownTabId(int tabId) {
            assert mTabIds.contains(tabId);
            mLastShownTabId = tabId;
        }

        int nextTabIdToShow(int tabId) {
            if (mTabIds.size() == 1 || !mTabIds.contains(tabId)) return Tab.INVALID_TAB_ID;
            List<Integer> ids = getTabIdList();
            int position = ids.indexOf(tabId);
            if (position == 0) return ids.get(position + 1);
            return ids.get(position - 1);
        }

        int getTabIdForIndex(int index) {
            return getTabIdList().get(index);
        }
    }

    private ObserverList<Observer> mGroupFilterObserver = new ObserverList<>();
    private Map<Integer, Integer> mGroupIdToGroupIndexMap = new HashMap<>();
    private Map<Integer, TabGroup> mGroupIdToGroupMap = new HashMap<>();
    private int mCurrentGroupIndex = TabList.INVALID_TAB_INDEX;
    // The number of groups with at least 2 tabs.
    private int mActualGroupCount;
    private Tab mAbsentSelectedTab;
    private boolean mShouldRecordUma = true;
    private boolean mIsResetting;
    private boolean mIsUndoing;

    public TabGroupModelFilter(TabModel tabModel) {
        super(tabModel);
    }

    /**
     * This method adds a {@link Observer} to be notified on {@link TabGroupModelFilter} changes.
     * @param observer The {@link Observer} to add.
     */
    public void addTabGroupObserver(Observer observer) {
        mGroupFilterObserver.addObserver(observer);
    }

    /**
     * This method removes a {@link Observer}.
     * @param observer The {@link Observer} to remove.
     */
    public void removeTabGroupObserver(Observer observer) {
        mGroupFilterObserver.removeObserver(observer);
    }

    /**
     * @return Number of {@link TabGroup}s that has at least two tabs.
     */
    public int getTabGroupCount() {
        return mActualGroupCount;
    }

    /**
     * This method moves the TabGroup which contains the Tab with TabId {@code id} to
     * {@code newIndex} in TabModel.
     * @param id         The id of the tab whose related tabs are being moved.
     * @param newIndex   The new index in TabModel that these tabs are being moved to.
     */
    public void moveRelatedTabs(int id, int newIndex) {
        List<Tab> tabs = getRelatedTabList(id);
        TabModel tabModel = getTabModel();
        newIndex = MathUtils.clamp(newIndex, 0, tabModel.getCount());
        int curIndex = TabModelUtils.getTabIndexById(tabModel, tabs.get(0).getId());

        if (curIndex == INVALID_TAB_INDEX || curIndex == newIndex) {
            return;
        }

        int offset = 0;
        for (Tab tab : tabs) {
            if (tabModel.indexOf(tab) == -1) {
                assert false : "Tried to close a tab from another model!";
                continue;
            }
            tabModel.moveTab(tab.getId(), newIndex >= curIndex ? newIndex : newIndex + offset++);
        }
    }

    /**
     * This method merges the source group that contains the {@code sourceTabId} to the destination
     * group that contains the {@code destinationTabId}. This method only operates if two groups are
     * in the same {@code TabModel}.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param destinationTabId The id of a {@link Tab} to get the destination group.
     */
    public void mergeTabsToGroup(int sourceTabId, int destinationTabId) {
        mergeTabsToGroup(sourceTabId, destinationTabId, false);
    }

    /**
     * This method merges the source group that contains the {@code sourceTabId} to the destination
     * group that contains the {@code destinationTabId}. This method only operates if two groups are
     * in the same {@code TabModel}.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param destinationTabId The id of a {@link Tab} to get the destination group.
     * @param skipUpdateTabModel True if updating the tab model will be handled elsewhere (e.g. by
     *                           the tab strip).
     */
    public void mergeTabsToGroup(
            int sourceTabId, int destinationTabId, boolean skipUpdateTabModel) {
        Tab sourceTab = TabModelUtils.getTabById(getTabModel(), sourceTabId);
        Tab destinationTab = TabModelUtils.getTabById(getTabModel(), destinationTabId);

        assert sourceTab != null
                        && destinationTab != null
                        && sourceTab.isIncognito() == destinationTab.isIncognito()
                : "Attempting to merge groups from different model";

        int destinationGroupId = getRootId(destinationTab);
        List<Tab> tabsToMerge = getRelatedTabList(sourceTabId);
        int destinationIndexInTabModel = getTabModelDestinationIndex(destinationTab);
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();
        String destinationGroupTitle = TabGroupTitleUtils.getTabGroupTitle(destinationGroupId);

        if (skipUpdateTabModel || !needToUpdateTabModel(tabsToMerge, destinationIndexInTabModel)) {
            for (Observer observer : mGroupFilterObserver) {
                observer.willMergeTabToGroup(
                        tabsToMerge.get(tabsToMerge.size() - 1), destinationGroupId);
            }
            for (int i = 0; i < tabsToMerge.size(); i++) {
                Tab tab = tabsToMerge.get(i);

                // Skip unnecessary work of populating the lists if logic is skipped below.
                if (!skipUpdateTabModel) {
                    int index = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
                    assert index != TabModel.INVALID_TAB_INDEX;
                    originalIndexes.add(index);
                    originalRootIds.add(getRootId(tab));
                }

                setRootId(tab, destinationGroupId);
            }
            resetFilterState();

            Tab lastMergedTab = tabsToMerge.get(tabsToMerge.size() - 1);
            TabGroup group = mGroupIdToGroupMap.get(getRootId(lastMergedTab));
            for (Observer observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(
                        tabsToMerge.get(tabsToMerge.size() - 1), group.getLastShownTabId());
                // Since the undo group merge logic is unsupported when called from the tab strip,
                // skip notifying the UndoGroupSnackbarController observer which shows the snackbar.
                if (!skipUpdateTabModel) {
                    observer.didCreateGroup(
                            tabsToMerge, originalIndexes, originalRootIds, destinationGroupTitle);
                }
            }
        } else {
            // For non adjacent tabs, the same logic as above applies regarding the tab strip skip.
            mergeListOfTabsToGroup(tabsToMerge, destinationTab, true, !skipUpdateTabModel);
        }
    }

    /**
     * This method appends a list of {@link Tab}s to the destination group that contains the
     * {@code} destinationTab. The {@link TabModel} ordering of the tabs in the given list is not
     * preserved. After calling this method, the {@link TabModel} ordering of these tabs would
     * become the ordering of {@code tabs}.
     *
     * @param tabs List of {@link Tab}s to be appended.
     * @param destinationTab The destination {@link Tab} to be append to.
     * @param isSameGroup Whether the given list of {@link Tab}s belongs in the same group
     *                    originally.
     * @param notify Whether or not to notify observers about the merging events.
     */
    public void mergeListOfTabsToGroup(
            List<Tab> tabs, Tab destinationTab, boolean isSameGroup, boolean notify) {
        int destinationGroupId = getRootId(destinationTab);
        int destinationIndexInTabModel = getTabModelDestinationIndex(destinationTab);
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();
        String destinationGroupTitle = TabGroupTitleUtils.getTabGroupTitle(destinationGroupId);

        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            // When merging tabs are in the same group, only make one willMergeTabToGroup call.
            if (!isSameGroup || i == tabs.size() - 1) {
                for (Observer observer : mGroupFilterObserver) {
                    observer.willMergeTabToGroup(tab, destinationGroupId);
                }
            }
            int index = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
            assert index != TabModel.INVALID_TAB_INDEX;
            originalIndexes.add(index);
            originalRootIds.add(getRootId(tab));

            if (tab.getId() == destinationTab.getId()) continue;

            boolean isMergingBackward = index < destinationIndexInTabModel;

            setRootId(tab, destinationGroupId);
            if (index == destinationIndexInTabModel || index + 1 == destinationIndexInTabModel) {
                // If the tab is not moved TabModelImpl will not invoke
                // TabModelObserver#didMoveTab() and update events will not be triggered. Call the
                // event manually.
                int destinationIndex =
                        MathUtils.clamp(
                                isMergingBackward
                                        ? destinationIndexInTabModel
                                        : destinationIndexInTabModel++,
                                0,
                                getTabModel().getCount());
                didMoveTab(tab, isMergingBackward ? destinationIndex - 1 : destinationIndex, index);
            } else {
                getTabModel()
                        .moveTab(
                                tab.getId(),
                                isMergingBackward
                                        ? destinationIndexInTabModel
                                        : destinationIndexInTabModel++);
            }
        }

        if (notify) {
            for (Observer observer : mGroupFilterObserver) {
                observer.didCreateGroup(
                        tabs, originalIndexes, originalRootIds, destinationGroupTitle);
            }
        }
    }

    /**
     * This method moves Tab with id as {@code sourceTabId} out of the group it belongs to in the
     * specified direction.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param trailing    True if the tab should be placed after the tab group when removed. False
     *                    if it should be placed before.
     */
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {
        TabModel tabModel = getTabModel();
        Tab sourceTab = TabModelUtils.getTabById(tabModel, sourceTabId);
        int sourceIndex = tabModel.indexOf(sourceTab);
        TabGroup sourceTabGroup = mGroupIdToGroupMap.get(getRootId(sourceTab));
        int targetIndex;
        if (trailing) {
            Tab lastTabInSourceGroup =
                    TabModelUtils.getTabById(
                            tabModel,
                            sourceTabGroup.getTabIdForIndex(
                                    sourceTabGroup.getTabIdList().size() - 1));
            targetIndex = tabModel.indexOf(lastTabInSourceGroup);
        } else {
            Tab firstTabInSourceGroup =
                    TabModelUtils.getTabById(tabModel, sourceTabGroup.getTabIdForIndex(0));
            targetIndex = tabModel.indexOf(firstTabInSourceGroup);
        }
        assert targetIndex != TabModel.INVALID_TAB_INDEX;

        int prevFilterIndex = mGroupIdToGroupIndexMap.get(getRootId(sourceTab));
        if (sourceTabGroup.size() == 1) {
            for (Observer observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(sourceTab, prevFilterIndex);
            }
            return;
        }
        int newRootId = getRootId(sourceTab);
        if (sourceTab.getId() == getRootId(sourceTab)) {
            // If moving tab's id is the root id of the group, find a new root id.
            if (sourceIndex != 0
                    && getRootId(tabModel.getTabAt(sourceIndex - 1)) == getRootId(sourceTab)) {
                newRootId = tabModel.getTabAt(sourceIndex - 1).getId();
            } else if (sourceIndex != tabModel.getCount() - 1
                    && getRootId(tabModel.getTabAt(sourceIndex + 1)) == getRootId(sourceTab)) {
                newRootId = tabModel.getTabAt(sourceIndex + 1).getId();
            }
        }
        assert newRootId != Tab.INVALID_TAB_ID;

        for (Observer observer : mGroupFilterObserver) {
            observer.willMoveTabOutOfGroup(sourceTab, newRootId);
        }
        if (sourceTab.getId() == getRootId(sourceTab)) {
            for (int tabId : sourceTabGroup.getTabIdList()) {
                setRootId(TabModelUtils.getTabById(tabModel, tabId), newRootId);
            }
            resetFilterState();
        }
        setRootId(sourceTab, sourceTab.getId());
        // If moving tab is already in the target index in tab model, no move in tab model.
        if (sourceIndex == targetIndex) {
            resetFilterState();
            for (Observer observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(sourceTab, prevFilterIndex);
            }
            return;
        }
        // Plus one as offset because we are moving backwards in tab model.
        tabModel.moveTab(sourceTab.getId(), trailing ? targetIndex + 1 : targetIndex);
    }

    /**
     * This method moves Tab with id as {@code sourceTabId} out of the group it belongs to.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     */
    public void moveTabOutOfGroup(int sourceTabId) {
        moveTabOutOfGroupInDirection(sourceTabId, true);
    }

    private int getTabModelDestinationIndex(Tab destinationTab) {
        List<Integer> destinationGroupedTabIds =
                mGroupIdToGroupMap.get(getRootId(destinationTab)).getTabIdList();
        int destinationTabIndex =
                TabModelUtils.getTabIndexById(
                        getTabModel(),
                        destinationGroupedTabIds.get(destinationGroupedTabIds.size() - 1));

        return destinationTabIndex + 1;
    }

    private boolean needToUpdateTabModel(List<Tab> tabsToMerge, int destinationIndexInTabModel) {
        assert tabsToMerge.size() > 0;

        int firstTabIndexInTabModel =
                TabModelUtils.getTabIndexById(getTabModel(), tabsToMerge.get(0).getId());
        return firstTabIndexInTabModel != destinationIndexInTabModel;
    }

    /**
     * This method undo the given grouped {@link Tab}.
     *
     * @param tab undo this grouped {@link Tab}.
     * @param originalIndex The tab index before grouped.
     * @param originalGroupId The rootId before grouped.
     */
    public void undoGroupedTab(Tab tab, int originalIndex, int originalGroupId) {
        if (!tab.isInitialized()) return;

        int currentIndex = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
        assert currentIndex != TabModel.INVALID_TAB_INDEX;

        // Unconditionally signal removal of the tab from the group it is in.
        mIsUndoing = true;
        boolean groupExistedBeforeMove = mGroupIdToGroupMap.get(originalGroupId) != null;
        setRootId(tab, originalGroupId);
        if (currentIndex == originalIndex) {
            didMoveTab(tab, originalIndex, currentIndex);
        } else {
            if (currentIndex < originalIndex) originalIndex++;
            getTabModel().moveTab(tab.getId(), originalIndex);
        }
        mIsUndoing = false;

        // If undoing results in restoring a tab into a different group (not as a single tab) then
        // notify observers it was added.
        if (groupExistedBeforeMove) {
            TabGroup group = mGroupIdToGroupMap.get(originalGroupId);
            // Last shown tab IDs are not preserved across an undo.
            for (Observer observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(tab, group.getLastShownTabId());
            }
        }
    }

    // TODO(crbug.com/951608): follow up with sessions count histogram for TabGroups.
    private int updateAndGetSessionsCount(int groupId) {
        ThreadUtils.assertOnBackgroundThread();

        String sessionsCountForGroupKey = SESSIONS_COUNT_FOR_GROUP + Integer.toString(groupId);
        SharedPreferences prefs = getSharedPreferences();
        int sessionsCount = prefs.getInt(sessionsCountForGroupKey, 0);
        sessionsCount++;
        prefs.edit().putInt(sessionsCountForGroupKey, sessionsCount).apply();
        return sessionsCount;
    }

    private SharedPreferences getSharedPreferences() {
        if (sPref == null) {
            sPref =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE);
        }
        return sPref;
    }

    // TabModelFilter implementation.
    @NonNull
    @Override
    public List<Tab> getRelatedTabList(int id) {
        // TODO(meiliang): In worst case, this method runs in O(n^2). This method needs to perform
        // better, especially when we try to call it in a loop for all tabs.
        Tab tab = TabModelUtils.getTabById(getTabModel(), id);
        if (tab == null) return super.getRelatedTabList(id);

        int groupId = getRootId(tab);
        TabGroup group = mGroupIdToGroupMap.get(groupId);
        if (group == null) return super.getRelatedTabList(TabModel.INVALID_TAB_INDEX);
        return getRelatedTabList(group.getTabIdList());
    }

    @Override
    public List<Integer> getRelatedTabIds(int tabId) {
        Tab tab = TabModelUtils.getTabById(getTabModel(), tabId);
        if (tab == null) return super.getRelatedTabIds(tabId);

        int groupId = getRootId(tab);
        TabGroup group = mGroupIdToGroupMap.get(groupId);
        if (group == null) return super.getRelatedTabIds(TabModel.INVALID_TAB_INDEX);
        return Collections.unmodifiableList(group.getTabIdList());
    }

    /**
     * This method returns all tabs in a tab group with reference to {@code tabRootId} as group id.
     *
     * @param tabRootId The tab root id that is used to find the related group.
     * @return An unmodifiable list of {@link Tab} that relate with the given tab root id.
     */
    public List<Tab> getRelatedTabListForRootId(int tabRootId) {
        if (tabRootId == Tab.INVALID_TAB_ID) return super.getRelatedTabList(tabRootId);
        TabGroup group = mGroupIdToGroupMap.get(tabRootId);
        if (group == null) return super.getRelatedTabList(TabModel.INVALID_TAB_INDEX);
        return getRelatedTabList(group.getTabIdList());
    }

    /**
     * This method returns the number of tabs in a tab group with reference to {@code tabRootId} as
     * group id.
     *
     * @param tabRootId The tab root id that is used to find the related group.
     * @return The number of related tabs.
     */
    public int getRelatedTabCountForRootId(int tabRootId) {
        if (tabRootId == Tab.INVALID_TAB_ID) return 1;
        TabGroup group = mGroupIdToGroupMap.get(tabRootId);
        if (group == null) return 1;
        return group.size();
    }

    @Override
    public boolean hasOtherRelatedTabs(Tab tab) {
        int groupId = getRootId(tab);
        TabGroup group = mGroupIdToGroupMap.get(groupId);
        return group != null && group.size() > 1;
    }

    private List<Tab> getRelatedTabList(List<Integer> ids) {
        List<Tab> tabs = new ArrayList<>();
        for (Integer id : ids) {
            Tab tab = TabModelUtils.getTabById(getTabModel(), id);
            // TODO(crbug/1382463): If this is called during a TabModelObserver observer iterator
            // it is possible a sequencing issue can occur where the tab is gone from the TabModel,
            // but still exists in the TabGroup. Avoid returning null by skipping the tab if it
            // doesn't exist in the TabModel.
            if (tab == null) continue;
            tabs.add(tab);
        }
        return Collections.unmodifiableList(tabs);
    }

    private int getParentId(Tab tab) {
        if (isTabModelRestored()
                && !mIsResetting
                && ((tab.getLaunchType() == TabLaunchType.FROM_TAB_GROUP_UI
                        || tab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP
                        // TODO(https://crbug.com/1194287): Investigates a better solution
                        // without adding the TabLaunchType.FROM_START_SURFACE.
                        || tab.getLaunchType() == TabLaunchType.FROM_START_SURFACE))) {
            Tab parentTab = TabModelUtils.getTabById(getTabModel(), tab.getParentId());
            if (parentTab != null) {
                return getRootId(parentTab);
            }
        }
        return Tab.INVALID_TAB_ID;
    }

    @Override
    protected void addTab(Tab tab) {
        if (tab.isIncognito() != isIncognito()) {
            throw new IllegalStateException("Attempting to open tab in the wrong model");
        }

        int parentId = getParentId(tab);
        if (parentId != Tab.INVALID_TAB_ID) {
            setRootId(tab, parentId);
        }

        int groupId = getRootId(tab);
        if (mGroupIdToGroupMap.containsKey(groupId)) {
            if (mGroupIdToGroupMap.get(groupId).size() == 1) {
                mActualGroupCount++;
                // TODO(crbug.com/1188370): Update UMA for Context menu creation.
                if (mShouldRecordUma
                        && (tab.getLaunchType()
                                == TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP)) {
                    RecordUserAction.record("TabGroup.Created.OpenInNewTab");
                }
            }
            mGroupIdToGroupMap.get(groupId).addTab(tab.getId());
        } else {
            TabGroup tabGroup = new TabGroup(getRootId(tab));
            tabGroup.addTab(tab.getId());
            mGroupIdToGroupMap.put(groupId, tabGroup);
            if (mIsResetting || getTabModel().indexOf(tab) == getTabModel().getCount() - 1) {
                // During a reset tabs are iterated over in TabModel order so it is safe to assume
                // group ordering matches tab ordering. Same is true if the new tab is the last tab
                // in the model.
                mGroupIdToGroupIndexMap.put(groupId, mGroupIdToGroupIndexMap.size());
            } else {
                // When adding a new tab that isn't at the end of the TabModel the new group's
                // index should be based on tab model order. This will offset all other groups
                // resulting in the index map needing to be regenerated.
                resetGroupIdToGroupIndexMap();
            }
        }

        if (mAbsentSelectedTab != null) {
            Tab absentSelectedTab = mAbsentSelectedTab;
            mAbsentSelectedTab = null;
            selectTab(absentSelectedTab);
        }
    }

    private void resetGroupIdToGroupIndexMap() {
        mGroupIdToGroupIndexMap.clear();
        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            int groupId = getRootId(tab);
            if (!mGroupIdToGroupIndexMap.containsKey(groupId)) {
                mGroupIdToGroupIndexMap.put(groupId, mGroupIdToGroupIndexMap.size());
            }
        }
    }

    @Override
    protected void closeTab(Tab tab) {
        int groupId = getRootId(tab);
        if (tab.isIncognito() != isIncognito()
                || mGroupIdToGroupMap.get(groupId) == null
                || !mGroupIdToGroupMap.get(groupId).contains(tab.getId())) {
            throw new IllegalStateException("Attempting to close tab in the wrong model");
        }

        TabGroup group = mGroupIdToGroupMap.get(groupId);
        group.removeTab(tab.getId());
        if (group.size() == 1) mActualGroupCount--;
        if (group.size() == 0) {
            updateGroupIdToGroupIndexMapAfterGroupClosed(groupId);
            mGroupIdToGroupIndexMap.remove(groupId);
            mGroupIdToGroupMap.remove(groupId);
            AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> removeGroupFromPref(groupId));
        }
    }

    private void removeGroupFromPref(int groupId) {
        ThreadUtils.assertOnBackgroundThread();

        SharedPreferences prefs = getSharedPreferences();
        String key = SESSIONS_COUNT_FOR_GROUP + Integer.toString(groupId);
        if (prefs.contains(key)) {
            prefs.edit().remove(key).apply();
        }
    }

    private void updateGroupIdToGroupIndexMapAfterGroupClosed(int groupId) {
        int indexToRemove = mGroupIdToGroupIndexMap.get(groupId);
        Set<Integer> groupIdSet = mGroupIdToGroupIndexMap.keySet();
        for (Integer groupIdKey : groupIdSet) {
            int groupIndex = mGroupIdToGroupIndexMap.get(groupIdKey);
            if (groupIndex > indexToRemove) {
                mGroupIdToGroupIndexMap.put(groupIdKey, groupIndex - 1);
            }
        }
    }

    @Override
    protected void selectTab(Tab tab) {
        assert mAbsentSelectedTab == null;
        int groupId = getRootId(tab);
        if (mGroupIdToGroupMap.get(groupId) == null) {
            mAbsentSelectedTab = tab;
        } else {
            mGroupIdToGroupMap.get(groupId).setLastShownTabId(tab.getId());
            mCurrentGroupIndex = mGroupIdToGroupIndexMap.get(groupId);
        }
    }

    @Override
    protected void reorder() {
        reorderGroup(TabGroup.INVALID_GROUP_ID);

        TabModel tabModel = getTabModel();
        if (tabModel.index() == TabModel.INVALID_TAB_INDEX) {
            mCurrentGroupIndex = TabModel.INVALID_TAB_INDEX;
        } else {
            selectTab(tabModel.getTabAt(tabModel.index()));
        }

        assert mGroupIdToGroupIndexMap.size() == mGroupIdToGroupMap.size();
    }

    private void reorderGroup(int groupId) {
        boolean reorderAllGroups = groupId == TabGroup.INVALID_GROUP_ID;
        if (reorderAllGroups) {
            mGroupIdToGroupIndexMap.clear();
        }

        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            if (reorderAllGroups) {
                groupId = getRootId(tab);
                if (!mGroupIdToGroupIndexMap.containsKey(groupId)) {
                    mGroupIdToGroupIndexMap.put(groupId, mGroupIdToGroupIndexMap.size());
                }
            }
            mGroupIdToGroupMap.get(groupId).moveToEndInGroup(tab.getId());
        }
    }

    @Override
    protected void resetFilterStateInternal() {
        mGroupIdToGroupIndexMap.clear();
        mGroupIdToGroupMap.clear();
        mActualGroupCount = 0;
    }

    @Override
    protected void removeTab(Tab tab) {
        closeTab(tab);
    }

    @Override
    protected void resetFilterState() {
        mShouldRecordUma = false;
        mIsResetting = true;
        Map<Integer, Integer> groupIdToGroupLastShownTabId = new HashMap<>();
        for (int groupId : mGroupIdToGroupMap.keySet()) {
            groupIdToGroupLastShownTabId.put(
                    groupId, mGroupIdToGroupMap.get(groupId).getLastShownTabId());
        }

        super.resetFilterState();

        // Restore previous last shown tab ids after resetting filter state.
        for (int groupId : mGroupIdToGroupMap.keySet()) {
            // This happens when group with new groupId is formed after resetting filter state, i.e.
            // when ungroup happens. Restoring last shown id of newly generated group is ignored.
            if (!groupIdToGroupLastShownTabId.containsKey(groupId)) continue;
            int lastShownId = groupIdToGroupLastShownTabId.get(groupId);
            // This happens during continuous resetFilterState() calls caused by merging multiple
            // tabs. Ignore the calls where the merge is not completed but the last shown tab has
            // already been merged to new group.
            if (!mGroupIdToGroupMap.get(groupId).contains(lastShownId)) continue;
            mGroupIdToGroupMap.get(groupId).setLastShownTabId(lastShownId);
        }
        TabModel tabModel = getTabModel();
        if (tabModel.index() == TabModel.INVALID_TAB_INDEX) {
            mCurrentGroupIndex = TabModel.INVALID_TAB_INDEX;
        } else {
            selectTab(tabModel.getTabAt(tabModel.index()));
        }
        mShouldRecordUma = true;
        mIsResetting = false;
    }

    @Override
    protected boolean shouldNotifyObserversOnSetIndex() {
        return mAbsentSelectedTab == null;
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void markTabStateInitialized() {
        super.markTabStateInitialized();
        boolean correctOrder = isOrderValid();
        RecordHistogram.recordBooleanHistogram("Tabs.Tasks.OrderValidOnStartup", correctOrder);
    }

    /**
     * Checks whether the order of the tabs in the {@link TabModel} respects the invariant of
     * {@link TabGroupModelFilter} that tabs within a group must be contiguous.
     *
     * Valid order:
     * Tab 1, Group A
     * Tab 2, Group A
     * Tab 3, Group B
     *
     * Invalid order:
     * Tab 1, Group A
     * Tab 2, Group B
     * Tab 3, Group A
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public boolean isOrderValid() {
        HashSet<Integer> processedRootIds = new HashSet<>();
        int lastRootId = Tab.INVALID_TAB_ID;
        // Iterate over tab model and check that all tabs with the same rootId are next to one
        // another. If at any time a rootId is repeated without the prior tab having the same rootId
        // then the invariant is violated.
        for (int i = 0; i < getTabModel().getCount(); i++) {
            int rootId = getRootId(getTabModel().getTabAt(i));
            if (rootId == lastRootId) continue;

            if (processedRootIds.contains(rootId)) return false;

            processedRootIds.add(lastRootId);
            lastRootId = rootId;
        }
        return true;
    }

    @Override
    public int getValidPosition(Tab tab, int proposedPosition) {
        final int parentId = getParentId(tab);
        final int rootId = parentId == Tab.INVALID_TAB_ID ? getRootId(tab) : parentId;
        int newPosition = proposedPosition;
        // If the tab is not in the model and won't be part of a group ensure it is positioned
        // outside any other groups.
        if (rootId == Tab.INVALID_TAB_ID || !mGroupIdToGroupMap.containsKey(rootId)) {
            newPosition = getValidPositionOfUngroupedTab(proposedPosition);
        } else {
            // The tab is or will be part of a group. Ensure it will be positioned with other
            // members of its group.
            TabGroup group = mGroupIdToGroupMap.get(rootId);
            newPosition = getValidPositionOfGroupedTab(group, proposedPosition);
        }
        RecordHistogram.recordBooleanHistogram(
                "Tabs.Tasks.TabAddedWithValidProposedPosition", newPosition == proposedPosition);
        return newPosition;
    }

    /**
     * Gets a valid position of a tab that will be part of a group. If proposedPosition is within
     * the range of the group's location it is used. Otherwise the tab is placed at the end of the
     * group.
     * @param group The group the tab belongs with.
     * @param proposedPosition The requested position of the tab.
     */
    private int getValidPositionOfGroupedTab(TabGroup group, int proposedPosition) {
        List<Integer> ids = new ArrayList<>();
        ids.addAll(group.getTabIdList());
        assert ids.size() >= 1;

        int firstGroupIndex = TabModelUtils.getTabIndexById(getTabModel(), ids.get(0));
        int defaultDestinationIndex = firstGroupIndex + ids.size();
        if (proposedPosition < firstGroupIndex) {
            return firstGroupIndex;
        }
        if (proposedPosition < defaultDestinationIndex) {
            return proposedPosition;
        }
        return defaultDestinationIndex;
    }

    /**
     * Gets a valid position of a tab that is not part of a group. If proposedPosition is not inside
     * any existing group it is used. Otherwise the tab is placed after the group it would have been
     * placed inside of.
     * @param proposedPosition The requested position of the tab.
     */
    private int getValidPositionOfUngroupedTab(int proposedPosition) {
        final int tabCount = getTabModel().getCount();
        if (proposedPosition <= 0 || proposedPosition >= tabCount) {
            // Downstream should clamp this value appropriately. Adding at the ends will never be a
            // problem.
            return proposedPosition;
        }

        int moveToIndex = proposedPosition;
        // Find a spot where the tabs on either side of the new tab are not part of the same group.
        while (moveToIndex != tabCount
                && getRootId(getTabModel().getTabAt(moveToIndex - 1))
                        == getRootId(getTabModel().getTabAt(moveToIndex))) {
            moveToIndex++;
        }
        return moveToIndex;
    }

    @Override
    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
        // Ignore didMoveTab calls in tab restoring stage.
        if (!isTabModelRestored()) return;
        // Need to cache the flags before resetting the internal data map.
        boolean isMergeTabToGroup = isMergeTabToGroup(tab);
        boolean isMoveTabOutOfGroup = isMoveTabOutOfGroup(tab) || mIsUndoing;
        int groupIdBeforeMove = getGroupIdBeforeMove(tab, isMergeTabToGroup || isMoveTabOutOfGroup);
        assert groupIdBeforeMove != TabGroup.INVALID_GROUP_ID;
        TabGroup groupBeforeMove = mGroupIdToGroupMap.get(groupIdBeforeMove);

        if (isMoveTabOutOfGroup) {
            resetFilterState();

            int prevFilterIndex = mGroupIdToGroupIndexMap.get(groupIdBeforeMove);
            for (Observer observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(tab, prevFilterIndex);
            }
        } else if (isMergeTabToGroup) {
            resetFilterState();
            if (groupBeforeMove != null && groupBeforeMove.size() != 1) return;

            TabGroup group = mGroupIdToGroupMap.get(getRootId(tab));
            for (Observer observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(tab, group.getLastShownTabId());
            }
        } else {
            reorder();
            if (isMoveWithinGroup(tab, curIndex, newIndex)) {
                for (Observer observer : mGroupFilterObserver) {
                    observer.didMoveWithinGroup(tab, curIndex, newIndex);
                }
            } else {
                if (!hasFinishedMovingGroup(tab, newIndex)) return;
                for (Observer observer : mGroupFilterObserver) {
                    observer.didMoveTabGroup(tab, curIndex, newIndex);
                }
            }
        }

        super.didMoveTab(tab, newIndex, curIndex);
    }

    private static void setRootId(Tab tab, int id) {
        tab.setRootId(id);
    }

    /**
     * Get the root id for the given tab. The root id is shared for tabs in the same group.
     * @param tab The {@link Tab}.
     * @return The root id for the given tab. The root id is shared for tabs in the same group.
     */
    public int getRootId(Tab tab) {
        return tab.getRootId();
    }

    private boolean isMoveTabOutOfGroup(Tab movedTab) {
        return !mGroupIdToGroupMap.containsKey(getRootId(movedTab));
    }

    private boolean isMergeTabToGroup(Tab tab) {
        if (!mGroupIdToGroupMap.containsKey(getRootId(tab))) return false;
        TabGroup tabGroup = mGroupIdToGroupMap.get(getRootId(tab));
        return !tabGroup.contains(tab.getId());
    }

    private int getGroupIdBeforeMove(Tab tabToMove, boolean isMoveToDifferentGroup) {
        if (!isMoveToDifferentGroup) return getRootId(tabToMove);

        Set<Integer> groupIdSet = mGroupIdToGroupMap.keySet();
        for (Integer groupIdKey : groupIdSet) {
            if (mGroupIdToGroupMap.get(groupIdKey).contains(tabToMove.getId())) {
                return groupIdKey;
            }
        }

        return TabGroup.INVALID_GROUP_ID;
    }

    private boolean isMoveWithinGroup(
            Tab movedTab, int oldIndexInTabModel, int newIndexInTabModel) {
        int startIndex = Math.min(oldIndexInTabModel, newIndexInTabModel);
        int endIndex = Math.max(oldIndexInTabModel, newIndexInTabModel);
        for (int i = startIndex; i <= endIndex; i++) {
            if (getRootId(getTabModel().getTabAt(i)) != getRootId(movedTab)) return false;
        }
        return true;
    }

    private boolean hasFinishedMovingGroup(Tab movedTab, int newIndexInTabModel) {
        TabGroup tabGroup = mGroupIdToGroupMap.get(getRootId(movedTab));
        int offsetIndex = newIndexInTabModel - tabGroup.size() + 1;
        if (offsetIndex < 0) return false;

        for (int i = newIndexInTabModel; i >= offsetIndex; i--) {
            if (getRootId(getTabModel().getTabAt(i)) != getRootId(movedTab)) return false;
        }
        return true;
    }

    // TabList implementation.
    @Override
    public boolean isIncognito() {
        return getTabModel().isIncognito();
    }

    @Override
    public int index() {
        return mCurrentGroupIndex;
    }

    /**
     * @return count of @{@link TabGroup}s in model.
     */
    @Override
    public int getCount() {
        return mGroupIdToGroupMap.size();
    }

    @Override
    public Tab getTabAt(int index) {
        if (index < 0 || index >= getCount()) return null;
        int groupId = Tab.INVALID_TAB_ID;
        Set<Integer> groupIdSet = mGroupIdToGroupIndexMap.keySet();
        for (Integer groupIdKey : groupIdSet) {
            if (mGroupIdToGroupIndexMap.get(groupIdKey) == index) {
                groupId = groupIdKey;
                break;
            }
        }
        if (groupId == Tab.INVALID_TAB_ID) return null;

        return TabModelUtils.getTabById(
                getTabModel(), mGroupIdToGroupMap.get(groupId).getLastShownTabId());
    }

    @Override
    public int indexOf(Tab tab) {
        if (tab == null
                || tab.isIncognito() != isIncognito()
                || getTabModel().indexOf(tab) == TabList.INVALID_TAB_INDEX) {
            return TabList.INVALID_TAB_INDEX;
        }

        int groupId = getRootId(tab);
        if (!mGroupIdToGroupIndexMap.containsKey(groupId)) return TabList.INVALID_TAB_INDEX;
        return mGroupIdToGroupIndexMap.get(groupId);
    }

    int getGroupLastShownTabIdForTesting(int groupId) {
        return mGroupIdToGroupMap.get(groupId).getLastShownTabId();
    }
}
