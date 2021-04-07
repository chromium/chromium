// Copyright 2019 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
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

    /**
     * An interface to be notified about changes to a {@link TabGroupModelFilter}.
     */
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
         * TabSelectionEditor (Group tab menu item) or using drag and drop.
         * @param tabs The list of modified {@link Tab}s.
         * @param tabOriginalIndex The original tab index for each modified tab.
         * @param isSameGroup Whether the given list is in a group already.
         */
        void didCreateGroup(List<Tab> tabs, List<Integer> tabOriginalIndex, boolean isSameGroup);
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

    // Create group automatically for target_blank links.
    private final boolean mGroupAutoCreation;

    public TabGroupModelFilter(TabModel tabModel) {
        this(tabModel, true);
    }

    public TabGroupModelFilter(TabModel tabModel, boolean autoCreation) {
        super(tabModel);
        mGroupAutoCreation = autoCreation;
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
     * This method records the number of sessions of the provided {@link Tab}, only if that
     * {@link Tab} is in a group that has at least two tab, and it records as
     * "TabGroups.SessionPerGroup".
     * @param tab {@link Tab}
     */
    public void recordSessionsCount(Tab tab) {
        int groupId = getRootId(tab);
        boolean isActualGroup = mGroupIdToGroupMap.get(groupId) != null
                && mGroupIdToGroupMap.get(groupId).size() > 1;
        if (!isActualGroup) return;

        AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> {
            int sessionsCount = updateAndGetSessionsCount(groupId);
            RecordHistogram.recordCountHistogram("TabGroups.SessionsPerGroup", sessionsCount);
        });
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
        Tab sourceTab = TabModelUtils.getTabById(getTabModel(), sourceTabId);
        Tab destinationTab = TabModelUtils.getTabById(getTabModel(), destinationTabId);

        assert sourceTab != null && destinationTab != null
                && sourceTab.isIncognito()
                        == destinationTab.isIncognito()
            : "Attempting to merge groups from different model";

        int destinationGroupId = getRootId(destinationTab);
        List<Tab> tabsToMerge = getRelatedTabList(sourceTabId);
        int destinationIndexInTabModel = getTabModelDestinationIndex(destinationTab);

        if (!needToUpdateTabModel(tabsToMerge, destinationIndexInTabModel)) {
            for (Observer observer : mGroupFilterObserver) {
                observer.willMergeTabToGroup(
                        tabsToMerge.get(tabsToMerge.size() - 1), destinationGroupId);
            }
            for (int i = 0; i < tabsToMerge.size(); i++) {
                setRootId(tabsToMerge.get(i), destinationGroupId);
            }
            resetFilterState();

            Tab lastMergedTab = tabsToMerge.get(tabsToMerge.size() - 1);
            TabGroup group = mGroupIdToGroupMap.get(getRootId(lastMergedTab));
            for (Observer observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(
                        tabsToMerge.get(tabsToMerge.size() - 1), group.getLastShownTabId());
            }
        } else {
            mergeListOfTabsToGroup(tabsToMerge, destinationTab, true, false);
        }
        // TODO(978508): Send didCreateGroup signal to activate the
        // {@link UndoGroupSnackbarController}.
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

            if (tab.getId() == destinationTab.getId()) continue;

            boolean isMergingBackward = index < destinationIndexInTabModel;

            setRootId(tab, destinationGroupId);
            getTabModel().moveTab(tab.getId(),
                    isMergingBackward ? destinationIndexInTabModel : destinationIndexInTabModel++);
        }

        if (notify) {
            for (Observer observer : mGroupFilterObserver) {
                observer.didCreateGroup(tabs, originalIndexes, isSameGroup);
            }
        }
    }

    /**
     * This method moves Tab with id as {@code sourceTabId} out of the group it belongs to.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     */
    public void moveTabOutOfGroup(int sourceTabId) {
        TabModel tabModel = getTabModel();
        Tab sourceTab = TabModelUtils.getTabById(tabModel, sourceTabId);
        int sourceIndex = tabModel.indexOf(sourceTab);
        TabGroup sourceTabGroup = mGroupIdToGroupMap.get(getRootId(sourceTab));
        Tab lastTabInSourceGroup = TabModelUtils.getTabById(tabModel,
                sourceTabGroup.getTabIdForIndex(sourceTabGroup.getTabIdList().size() - 1));
        int targetIndex = tabModel.indexOf(lastTabInSourceGroup);
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
        tabModel.moveTab(sourceTab.getId(), targetIndex + 1);
    }

    private int getTabModelDestinationIndex(Tab destinationTab) {
        List<Integer> destinationGroupedTabIds =
                mGroupIdToGroupMap.get(getRootId(destinationTab)).getTabIdList();
        int destinationTabIndex = TabModelUtils.getTabIndexById(
                getTabModel(), destinationGroupedTabIds.get(destinationGroupedTabIds.size() - 1));

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
        if (!tab.isInitialized()) {
            return;
        }
        int currentIndex = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
        assert currentIndex != TabModel.INVALID_TAB_INDEX;

        setRootId(tab, originalGroupId);
        if (currentIndex == originalIndex) {
            didMoveTab(tab, originalIndex, currentIndex);
        } else {
            if (currentIndex < originalIndex) originalIndex++;
            getTabModel().moveTab(tab.getId(), originalIndex);
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
            sPref = ContextUtils.getApplicationContext().getSharedPreferences(
                    PREFS_FILE, Context.MODE_PRIVATE);
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

    @Override
    public boolean hasOtherRelatedTabs(Tab tab) {
        int groupId = getRootId(tab);
        TabGroup group = mGroupIdToGroupMap.get(groupId);
        return group != null && group.size() > 1;
    }

    private List<Tab> getRelatedTabList(List<Integer> ids) {
        List<Tab> tabs = new ArrayList<>();
        for (Integer id : ids) {
            tabs.add(TabModelUtils.getTabById(getTabModel(), id));
        }
        return Collections.unmodifiableList(tabs);
    }

    @Override
    protected void addTab(Tab tab) {
        if (tab.isIncognito() != isIncognito()) {
            throw new IllegalStateException("Attempting to open tab in the wrong model");
        }

        if (isTabModelRestored() && !mIsResetting
                && (mGroupAutoCreation
                        || (tab.getLaunchType() == TabLaunchType.FROM_TAB_GROUP_UI
                                || tab.getLaunchType()
                                        == TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP))) {
            Tab parentTab = TabModelUtils.getTabById(
                    getTabModel(), CriticalPersistedTabData.from(tab).getParentId());
            if (parentTab != null) {
                setRootId(tab, getRootId(parentTab));
            }
        }

        int groupId = getRootId(tab);
        if (mGroupIdToGroupMap.containsKey(groupId)) {
            if (mGroupIdToGroupMap.get(groupId).size() == 1) {
                mActualGroupCount++;
                // TODO(crbug.com/1188370): Update UMA for Context menu creation.
                if (mShouldRecordUma && mGroupAutoCreation
                        && tab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
                    RecordUserAction.record("TabGroup.Created.OpenInNewTab");
                }
            }
            mGroupIdToGroupMap.get(groupId).addTab(tab.getId());
        } else {
            TabGroup tabGroup = new TabGroup(getRootId(tab));
            tabGroup.addTab(tab.getId());
            mGroupIdToGroupMap.put(groupId, tabGroup);
            mGroupIdToGroupIndexMap.put(groupId, mGroupIdToGroupIndexMap.size());
        }

        if (mAbsentSelectedTab != null) {
            Tab absentSelectedTab = mAbsentSelectedTab;
            mAbsentSelectedTab = null;
            selectTab(absentSelectedTab);
        }
    }

    @Override
    protected void closeTab(Tab tab) {
        int groupId = getRootId(tab);
        if (tab.isIncognito() != isIncognito() || mGroupIdToGroupMap.get(groupId) == null
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
    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
        // Ignore didMoveTab calls in tab restoring stage.
        if (!isTabModelRestored()) return;
        // Need to cache the flags before resetting the internal data map.
        boolean isMergeTabToGroup = isMergeTabToGroup(tab);
        boolean isMoveTabOutOfGroup = isMoveTabOutOfGroup(tab);
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
        CriticalPersistedTabData.from(tab).setRootId(id);
    }

    private static int getRootId(Tab tab) {
        return CriticalPersistedTabData.from(tab).getRootId();
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
        if (tab == null || tab.isIncognito() != isIncognito()
                || getTabModel().indexOf(tab) == TabList.INVALID_TAB_INDEX) {
            return TabList.INVALID_TAB_INDEX;
        }

        int groupId = getRootId(tab);
        if (!mGroupIdToGroupIndexMap.containsKey(groupId)) return TabList.INVALID_TAB_INDEX;
        return mGroupIdToGroupIndexMap.get(groupId);
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return getTabModel().isClosurePending(tabId);
    }

    @VisibleForTesting
    int getGroupLastShownTabIdForTesting(int groupId) {
        return mGroupIdToGroupMap.get(groupId).getLastShownTabId();
    }
}
