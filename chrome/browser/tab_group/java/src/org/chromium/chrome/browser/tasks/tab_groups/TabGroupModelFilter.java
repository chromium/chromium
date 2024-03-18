// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * An implementation of {@link TabModelFilter} that puts {@link Tab}s into a group structure.
 *
 * <p>A group is a collection of {@link Tab}s that share a common ancestor {@link Tab}. This filter
 * is also a {@link TabList} that contains the last shown {@link Tab} from every group.
 *
 * <p>Note this class is in the process of migrating from RootId to TabGroupId. All references to
 * root ID refer to the old ID system. References to tab group ID will refer to the new system. See
 * https://crbug.com/1523745.
 */
public class TabGroupModelFilter extends TabModelFilter {
    private static final int INVALID_COLOR_ID = -1;

    private ObserverList<TabGroupModelFilterObserver> mGroupFilterObserver = new ObserverList<>();
    private Map<Integer, Integer> mRootIdToGroupIndexMap = new HashMap<>();
    private Map<Integer, TabGroup> mRootIdToGroupMap = new HashMap<>();
    private int mCurrentGroupIndex = TabList.INVALID_TAB_INDEX;
    // The number of tab groups with 2 tabs or a token based tab group ID.
    private int mActualGroupCount;
    private Tab mAbsentSelectedTab;
    private boolean mShouldRecordUma = true;
    private boolean mIsResetting;
    private boolean mIsUndoing;

    public TabGroupModelFilter(TabModel tabModel) {
        super(tabModel);
    }

    /**
     * This method adds a {@link TabGroupModelFilterObserver} to be notified on {@link
     * TabGroupModelFilter} changes.
     *
     * @param observer The {@link TabGroupModelFilterObserver} to add.
     */
    public void addTabGroupObserver(TabGroupModelFilterObserver observer) {
        mGroupFilterObserver.addObserver(observer);
    }

    /**
     * This method removes a {@link TabGroupModelFilterObserver}.
     *
     * @param observer The {@link TabGroupModelFilterObserver} to remove.
     */
    public void removeTabGroupObserver(TabGroupModelFilterObserver observer) {
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

        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.willMoveTabGroup(curIndex, newIndex);
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

    /** Creates a tab group containing a single tab. */
    public void createSingleTabGroup(int tabId, boolean notify) {
        createSingleTabGroup(TabModelUtils.getTabById(getTabModel(), tabId), notify);
    }

    /** Creates a tab group containing a single tab. */
    public void createSingleTabGroup(Tab tab, boolean notify) {
        assert ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled();
        assert tab.getTabGroupId() == null;
        tab.setTabGroupId(Token.createRandom());
        mActualGroupCount++;
        boolean didCreateNewGroup = true;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)) {
            // If the destination group already has an assigned color, then this action is not
            // for a new tab group creation. Currently, the only case where this would be called
            // and it is not a new tab group creation is when a tab group is restored from the
            // recent tabs page, where the color will be set before this call.
            int destinationGroupColorId = TabGroupColorUtils.getTabGroupColor(tab.getRootId());
            didCreateNewGroup = didCreateNewGroup && (destinationGroupColorId == INVALID_COLOR_ID);
        }

        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            if (didCreateNewGroup) {
                observer.didCreateNewGroup(tab, this);
            }
        }

        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didMergeTabToGroup(tab, tab.getId());
        }

        if (notify) {
            int index = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didCreateGroup(
                        Collections.singletonList(tab),
                        Collections.singletonList(index),
                        Collections.singletonList(tab.getRootId()),
                        Collections.singletonList(null),
                        null,
                        INVALID_COLOR_ID);
            }
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

        List<Tab> tabsToMerge = getRelatedTabList(sourceTabId);
        int destinationIndexInTabModel = getTabModelDestinationIndex(destinationTab);

        if (!skipUpdateTabModel && needToUpdateTabModel(tabsToMerge, destinationIndexInTabModel)) {
            mergeListOfTabsToGroup(tabsToMerge, destinationTab, true, !skipUpdateTabModel);
        } else {
            int destinationRootId = destinationTab.getRootId();
            List<Tab> tabsIncludingDestination = new ArrayList<>();
            List<Integer> originalIndexes = new ArrayList<>();
            List<Integer> originalRootIds = new ArrayList<>();
            List<Token> originalTabGroupIds = new ArrayList<>();
            String destinationGroupTitle = TabGroupTitleUtils.getTabGroupTitle(destinationRootId);
            int destinationGroupColorId = INVALID_COLOR_ID;
            boolean didCreateNewGroup =
                    !isTabInTabGroup(sourceTab) && !isTabInTabGroup(destinationTab);

            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)) {
                destinationGroupColorId = TabGroupColorUtils.getTabGroupColor(destinationRootId);
                // If the destination group already has an assigned color, then this action is not
                // for a new tab group creation. Currently, the only case where this would be called
                // and it is not a new tab group creation is when a tab group is restored from the
                // recent tabs page, where the color will be set before this call.
                didCreateNewGroup =
                        didCreateNewGroup && (destinationGroupColorId == INVALID_COLOR_ID);
            }

            if (!skipUpdateTabModel) {
                tabsIncludingDestination.add(destinationTab);
                originalIndexes.add(
                        TabModelUtils.getTabIndexById(getTabModel(), destinationTab.getId()));
                originalRootIds.add(destinationRootId);
                originalTabGroupIds.add(destinationTab.getTabGroupId());
            }

            Token destinationTabGroupId =
                    getOrCreateTabGroupIdWithDefault(destinationTab, sourceTab.getTabGroupId());

            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.willMergeTabToGroup(
                        tabsToMerge.get(tabsToMerge.size() - 1), destinationRootId);
            }
            for (int i = 0; i < tabsToMerge.size(); i++) {
                Tab tab = tabsToMerge.get(i);

                // Skip unnecessary work of populating the lists if logic is skipped below.
                if (!skipUpdateTabModel) {
                    int index = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
                    assert index != TabModel.INVALID_TAB_INDEX;
                    tabsIncludingDestination.add(tab);
                    originalIndexes.add(index);
                    originalRootIds.add(tab.getRootId());
                    originalTabGroupIds.add(tab.getTabGroupId());
                }

                tab.setRootId(destinationRootId);
                tab.setTabGroupId(destinationTabGroupId);
            }
            resetFilterState();

            Tab lastMergedTab = tabsToMerge.get(tabsToMerge.size() - 1);
            TabGroup group = mRootIdToGroupMap.get(lastMergedTab.getRootId());
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(
                        tabsToMerge.get(tabsToMerge.size() - 1), group.getLastShownTabId());

                if (didCreateNewGroup) {
                    observer.didCreateNewGroup(destinationTab, this);
                }

                // Since the undo group merge logic is unsupported when called from the tab strip,
                // skip notifying the UndoGroupSnackbarController observer which shows the snackbar.
                if (!skipUpdateTabModel) {
                    observer.didCreateGroup(
                            tabsIncludingDestination,
                            originalIndexes,
                            originalRootIds,
                            originalTabGroupIds,
                            destinationGroupTitle,
                            destinationGroupColorId);
                }
            }
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
        // Check whether the destination tab is in a tab group before getOrCreateTabGroupId so we
        // send the correct signal for whether a tab group was newly created.
        boolean didCreateNewGroup = !isTabInTabGroup(destinationTab);
        List<Tab> mergedTabs = new ArrayList<>();
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();
        List<Token> originalTabGroupIds = new ArrayList<>();

        // Include the destination tab in the undo list so that it gets back a null tab group ID
        // upon undo if it didn't have one.
        int destinationRootId = destinationTab.getRootId();
        int destinationTabIndex =
                TabModelUtils.getTabIndexById(getTabModel(), destinationTab.getId());
        assert destinationTabIndex != TabModel.INVALID_TAB_INDEX;
        mergedTabs.add(destinationTab);
        originalIndexes.add(destinationTabIndex);
        originalRootIds.add(destinationRootId);
        originalTabGroupIds.add(destinationTab.getTabGroupId());

        Token destinationTabGroupId;
        if (isTabInTabGroup(destinationTab)) {
            destinationTabGroupId = destinationTab.getTabGroupId();
        } else {
            Token mergedTabGroupId = null;
            for (Tab tab : tabs) {
                mergedTabGroupId = tab.getTabGroupId();
                if (mergedTabGroupId != null) break;
            }
            destinationTabGroupId =
                    getOrCreateTabGroupIdWithDefault(destinationTab, mergedTabGroupId);
        }
        int destinationIndexInTabModel = getTabModelDestinationIndex(destinationTab);
        String destinationGroupTitle = TabGroupTitleUtils.getTabGroupTitle(destinationRootId);
        int destinationGroupColorId = INVALID_COLOR_ID;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)) {
            destinationGroupColorId = TabGroupColorUtils.getTabGroupColor(destinationRootId);
        }

        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);

            // Check if any of the tabs in the tab list are part of a tab group.
            if (didCreateNewGroup && isTabInTabGroup(tab)) {
                didCreateNewGroup = false;
            }

            // When merging tabs are in the same group, only make one willMergeTabToGroup call.
            if (!isSameGroup || i == tabs.size() - 1) {
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.willMergeTabToGroup(tab, destinationRootId);
                }
            }

            if (tab.getId() == destinationTab.getId()) continue;

            int index = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
            assert index != TabModel.INVALID_TAB_INDEX;

            mergedTabs.add(tab);
            originalIndexes.add(index);
            originalRootIds.add(tab.getRootId());
            originalTabGroupIds.add(tab.getTabGroupId());

            boolean isMergingBackward = index < destinationIndexInTabModel;

            tab.setRootId(destinationRootId);
            tab.setTabGroupId(destinationTabGroupId);
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

        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            if (didCreateNewGroup) {
                observer.didCreateNewGroup(destinationTab, this);
            }

            if (notify) {
                observer.didCreateGroup(
                        mergedTabs,
                        originalIndexes,
                        originalRootIds,
                        originalTabGroupIds,
                        destinationGroupTitle,
                        destinationGroupColorId);
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
        TabGroup sourceTabGroup = mRootIdToGroupMap.get(sourceTab.getRootId());

        int prevFilterIndex = mRootIdToGroupIndexMap.get(sourceTab.getRootId());
        if (sourceTabGroup.size() == 1) {
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.willMoveTabOutOfGroup(sourceTab, sourceTab.getRootId());
            }
            // When moving the last tab out of a tab group of size 1 we should decremement the
            // number of tab groups.
            if (ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()
                    && sourceTab.getTabGroupId() != null) {
                mActualGroupCount--;
            }
            sourceTab.setTabGroupId(null);
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(sourceTab, prevFilterIndex);
            }
            return;
        }

        int targetIndex;
        if (trailing) {
            Tab lastTabInSourceGroup =
                    TabModelUtils.getTabById(tabModel, sourceTabGroup.getTabIdOfLastTab());
            targetIndex = tabModel.indexOf(lastTabInSourceGroup);
        } else {
            Tab firstTabInSourceGroup =
                    TabModelUtils.getTabById(tabModel, sourceTabGroup.getTabIdOfFirstTab());
            targetIndex = tabModel.indexOf(firstTabInSourceGroup);
        }
        assert targetIndex != TabModel.INVALID_TAB_INDEX;

        int newRootId = sourceTab.getRootId();
        boolean sourceTabIdWasRootId = sourceTab.getId() == newRootId;
        if (sourceTabIdWasRootId) {
            // If moving tab's id is the root id of the group, find a new root id.
            if (sourceIndex != 0 && tabModel.getTabAt(sourceIndex - 1).getRootId() == newRootId) {
                newRootId = tabModel.getTabAt(sourceIndex - 1).getId();
            } else if (sourceIndex != tabModel.getCount() - 1
                    && tabModel.getTabAt(sourceIndex + 1).getRootId() == newRootId) {
                newRootId = tabModel.getTabAt(sourceIndex + 1).getId();
            }
        }
        assert newRootId != Tab.INVALID_TAB_ID;

        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.willMoveTabOutOfGroup(sourceTab, newRootId);
        }

        sourceTab.setTabGroupId(null);
        if (sourceTabIdWasRootId) {
            for (int tabId : sourceTabGroup.getTabIdList()) {
                Tab tab = TabModelUtils.getTabById(tabModel, tabId);
                tab.setRootId(newRootId);
            }
            resetFilterState();
        }
        sourceTab.setRootId(sourceTab.getId());
        // If moving tab is already in the target index in tab model, no move in tab model.
        if (sourceIndex == targetIndex) {
            resetFilterState();
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
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
                mRootIdToGroupMap.get(destinationTab.getRootId()).getTabIdList();
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
     * @param originalRootId The rootId before grouped.
     * @param originalTabGroupId The tabGroupId before grouped.
     */
    public void undoGroupedTab(
            Tab tab, int originalIndex, int originalRootId, @Nullable Token originalTabGroupId) {
        if (!tab.isInitialized()) return;

        int currentIndex = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
        assert currentIndex != TabModel.INVALID_TAB_INDEX;

        // Unconditionally signal removal of the tab from the group it is in.
        mIsUndoing = true;
        boolean groupExistedBeforeMove = mRootIdToGroupMap.get(originalRootId) != null;
        tab.setRootId(originalRootId);
        tab.setTabGroupId(originalTabGroupId);
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
            TabGroup group = mRootIdToGroupMap.get(originalRootId);
            // Last shown tab IDs are not preserved across an undo.
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(tab, group.getLastShownTabId());
            }
        }
    }


    // TabModelFilter implementation.
    @NonNull
    @Override
    public List<Tab> getRelatedTabList(int id) {
        // TODO(meiliang): In worst case, this method runs in O(n^2). This method needs to perform
        // better, especially when we try to call it in a loop for all tabs.
        Tab tab = TabModelUtils.getTabById(getTabModel(), id);
        if (tab == null) return super.getRelatedTabList(id);

        int rootId = tab.getRootId();
        TabGroup group = mRootIdToGroupMap.get(rootId);
        if (group == null) return super.getRelatedTabList(TabModel.INVALID_TAB_INDEX);
        return getRelatedTabList(group.getTabIdList());
    }

    @Override
    public List<Integer> getRelatedTabIds(int tabId) {
        Tab tab = TabModelUtils.getTabById(getTabModel(), tabId);
        if (tab == null) return super.getRelatedTabIds(tabId);

        int rootId = tab.getRootId();
        TabGroup group = mRootIdToGroupMap.get(rootId);
        if (group == null) return super.getRelatedTabIds(TabModel.INVALID_TAB_INDEX);
        return Collections.unmodifiableList(group.getTabIdList());
    }

    /**
     * This method returns all tabs in a tab group with reference to {@code tabRootId} as root id.
     *
     * @param tabRootId The tab root id that is used to find the related group.
     * @return An unmodifiable list of {@link Tab} that relate with the given tab root id.
     */
    public List<Tab> getRelatedTabListForRootId(int tabRootId) {
        if (tabRootId == Tab.INVALID_TAB_ID) return super.getRelatedTabList(tabRootId);
        TabGroup group = mRootIdToGroupMap.get(tabRootId);
        if (group == null) return super.getRelatedTabList(TabModel.INVALID_TAB_INDEX);
        return getRelatedTabList(group.getTabIdList());
    }

    /**
     * This method returns the number of tabs in a tab group with reference to {@code tabRootId} as
     * root id.
     *
     * @param tabRootId The tab root id that is used to find the related group.
     * @return The number of related tabs.
     */
    public int getRelatedTabCountForRootId(int tabRootId) {
        if (tabRootId == Tab.INVALID_TAB_ID) return 1;
        TabGroup group = mRootIdToGroupMap.get(tabRootId);
        if (group == null) return 1;
        return group.size();
    }

    @Override
    public boolean isTabInTabGroup(Tab tab) {
        int rootId = tab.getRootId();
        TabGroup group = mRootIdToGroupMap.get(rootId);
        boolean isInGroup = group != null && group.contains(tab.getId());
        if (ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()) {
            return isInGroup && tab.getTabGroupId() != null;
        } else {
            return isInGroup && group.size() > 1;
        }
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

    private boolean shouldUseParentIds(Tab tab) {
        return isTabModelRestored()
                && !mIsResetting
                && ((tab.getLaunchType() == TabLaunchType.FROM_TAB_GROUP_UI
                        || tab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP
                        // TODO(https://crbug.com/1194287): Investigates a better solution
                        // without adding the TabLaunchType.FROM_START_SURFACE.
                        || tab.getLaunchType() == TabLaunchType.FROM_START_SURFACE));
    }

    private Tab getParentTab(Tab tab) {
        return TabModelUtils.getTabById(getTabModel(), tab.getParentId());
    }

    @Override
    protected void addTab(Tab tab) {
        if (tab.isIncognito() != isIncognito()) {
            throw new IllegalStateException("Attempting to open tab in the wrong model");
        }

        boolean didCreateNewGroup = false;
        if (shouldUseParentIds(tab)) {
            Tab parentTab = getParentTab(tab);
            if (parentTab != null) {
                Token oldTabGroupId = parentTab.getTabGroupId();
                Token newTabGroupId = getOrCreateTabGroupId(parentTab);
                if (!Objects.equals(oldTabGroupId, newTabGroupId)) {
                    didCreateNewGroup = true;
                }
                tab.setRootId(parentTab.getRootId());
                tab.setTabGroupId(newTabGroupId);
            }
        }

        int rootId = tab.getRootId();
        if (mRootIdToGroupMap.containsKey(rootId)) {
            TabGroup group = mRootIdToGroupMap.get(rootId);
            if (!ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()) {
                didCreateNewGroup = group.size() == 1;
            }
            mRootIdToGroupMap.get(rootId).addTab(tab.getId(), getTabModel());

            if (didCreateNewGroup) {
                mActualGroupCount++;
                // TODO(crbug.com/1188370): Update UMA for Context menu creation.
                if (tab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP) {
                    if (mShouldRecordUma) {
                        RecordUserAction.record("TabGroup.Created.OpenInNewTab");
                    }

                    // When creating a tab group with the context menu longpress, this action runs.
                    for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                        observer.didCreateNewGroup(tab, this);
                    }
                }
            }
        } else {
            TabGroup tabGroup = new TabGroup(tab.getRootId());
            tabGroup.addTab(tab.getId(), getTabModel());
            mRootIdToGroupMap.put(rootId, tabGroup);
            if (mIsResetting || getTabModel().indexOf(tab) == getTabModel().getCount() - 1) {
                // During a reset tabs are iterated over in TabModel order so it is safe to assume
                // group ordering matches tab ordering. Same is true if the new tab is the last tab
                // in the model.
                mRootIdToGroupIndexMap.put(rootId, mRootIdToGroupIndexMap.size());
            } else {
                // When adding a new tab that isn't at the end of the TabModel the new group's
                // index should be based on tab model order. This will offset all other groups
                // resulting in the index map needing to be regenerated.
                resetRootIdToGroupIndexMap();
            }
            if (isTabInTabGroup(tab)) {
                mActualGroupCount++;
            }
        }

        if (mAbsentSelectedTab != null) {
            Tab absentSelectedTab = mAbsentSelectedTab;
            mAbsentSelectedTab = null;
            selectTab(absentSelectedTab);
        }
    }

    private void resetRootIdToGroupIndexMap() {
        mRootIdToGroupIndexMap.clear();
        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            int rootId = tab.getRootId();
            if (!mRootIdToGroupIndexMap.containsKey(rootId)) {
                mRootIdToGroupIndexMap.put(rootId, mRootIdToGroupIndexMap.size());
            }
        }
    }

    @Override
    protected void closeTab(Tab tab) {
        int rootId = tab.getRootId();
        if (tab.isIncognito() != isIncognito()
                || mRootIdToGroupMap.get(rootId) == null
                || !mRootIdToGroupMap.get(rootId).contains(tab.getId())) {
            throw new IllegalStateException("Attempting to close tab in the wrong model");
        }

        TabGroup group = mRootIdToGroupMap.get(rootId);
        group.removeTab(tab.getId());
        if (ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()) {
            if (group.size() == 0) mActualGroupCount--;
        } else {
            if (group.size() == 1) mActualGroupCount--;
        }
        if (group.size() == 0) {
            updateRootIdToGroupIndexMapAfterGroupClosed(rootId);
            mRootIdToGroupIndexMap.remove(rootId);
            mRootIdToGroupMap.remove(rootId);
        }
    }

    private void updateRootIdToGroupIndexMapAfterGroupClosed(int rootId) {
        int indexToRemove = mRootIdToGroupIndexMap.get(rootId);
        Set<Integer> rootIdSet = mRootIdToGroupIndexMap.keySet();
        for (Integer rootIdKey : rootIdSet) {
            int groupIndex = mRootIdToGroupIndexMap.get(rootIdKey);
            if (groupIndex > indexToRemove) {
                mRootIdToGroupIndexMap.put(rootIdKey, groupIndex - 1);
            }
        }
    }

    @Override
    protected void selectTab(Tab tab) {
        assert mAbsentSelectedTab == null;
        int rootId = tab.getRootId();
        if (mRootIdToGroupMap.get(rootId) == null) {
            mAbsentSelectedTab = tab;
        } else {
            mRootIdToGroupMap.get(rootId).setLastShownTabId(tab.getId());
            mCurrentGroupIndex = mRootIdToGroupIndexMap.get(rootId);
        }
    }

    @Override
    protected void reorder() {
        mRootIdToGroupIndexMap.clear();
        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            int rootId = tab.getRootId();
            if (!mRootIdToGroupIndexMap.containsKey(rootId)) {
                mRootIdToGroupIndexMap.put(rootId, mRootIdToGroupIndexMap.size());
            }
            mRootIdToGroupMap.get(rootId).moveToEndInGroup(tab.getId());
        }

        if (tabModel.index() == TabModel.INVALID_TAB_INDEX) {
            mCurrentGroupIndex = TabModel.INVALID_TAB_INDEX;
        } else {
            selectTab(tabModel.getTabAt(tabModel.index()));
        }

        assert mRootIdToGroupIndexMap.size() == mRootIdToGroupMap.size();
    }

    @Override
    protected void resetFilterStateInternal() {
        mRootIdToGroupIndexMap.clear();
        mRootIdToGroupMap.clear();
        mActualGroupCount = 0;
    }

    @Override
    protected void removeTab(Tab tab) {
        closeTab(tab);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @Override
    public void resetFilterState() {
        mShouldRecordUma = false;
        mIsResetting = true;
        Map<Integer, Integer> rootIdToGroupLastShownTabId = new HashMap<>();
        for (int rootId : mRootIdToGroupMap.keySet()) {
            rootIdToGroupLastShownTabId.put(
                    rootId, mRootIdToGroupMap.get(rootId).getLastShownTabId());
        }

        super.resetFilterState();

        // Restore previous last shown tab ids after resetting filter state.
        for (int rootId : mRootIdToGroupMap.keySet()) {
            // This happens when group with new rootId is formed after resetting filter state, i.e.
            // when ungroup happens. Restoring last shown id of newly generated group is ignored.
            if (!rootIdToGroupLastShownTabId.containsKey(rootId)) continue;
            int lastShownId = rootIdToGroupLastShownTabId.get(rootId);
            // This happens during continuous resetFilterState() calls caused by merging multiple
            // tabs. Ignore the calls where the merge is not completed but the last shown tab has
            // already been merged to new group.
            if (!mRootIdToGroupMap.get(rootId).contains(lastShownId)) continue;
            mRootIdToGroupMap.get(rootId).setLastShownTabId(lastShownId);
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

        int fixedRootIdCount = fixRootIds();
        RecordHistogram.recordCount1000Histogram(
                "TabGroups.NumberOfRootIdsFixed", fixedRootIdCount);

        if (ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()) {
            addTabGroupIdsForAllTabGroups();
        } else {
            removeTabGroupIdsForAllTabGroups();
        }
    }

    @VisibleForTesting
    void addTabGroupIdsForAllTabGroups() {
        TabModel model = getTabModel();
        @Nullable Token lastTabGroupId = null;
        int lastRootId = TabGroup.INVALID_ROOT_ID;

        // Assume all tab groups are contiguous.
        for (int i = 0; i < model.getCount(); i++) {
            Tab tab = model.getTabAt(i);
            int rootId = tab.getRootId();
            Token tabGroupId = tab.getTabGroupId();
            TabGroup group = mRootIdToGroupMap.get(rootId);

            if (rootId == lastRootId) {
                // The tab is part of previous tab's group it should get the tab group ID from it.
                assert lastTabGroupId != null;
                tabGroupId = lastTabGroupId;
                tab.setTabGroupId(tabGroupId);
            } else if (tabGroupId == null && group.size() > 1) {
                // The tab does not have a tab group ID, but is part of a > 1 size group. Assign it
                // a new tab group ID.
                tabGroupId = Token.createRandom();
                tab.setTabGroupId(tabGroupId);
            }
            // Remaining cases:
            // * A tab group of size 1 is not migrated. It either has a null ID or tab group ID.
            // * A tab group > size 1 that already has a tab group ID should not change IDs.

            lastRootId = rootId;
            lastTabGroupId = tabGroupId;
        }
    }

    @VisibleForTesting
    void removeTabGroupIdsForAllTabGroups() {
        TabModel model = getTabModel();
        for (int i = 0; i < model.getCount(); i++) {
            Tab tab = model.getTabAt(i);
            tab.setTabGroupId(null);
        }
    }

    /**
     * Checks whether the order of the tabs in the {@link TabModel} respects the invariant of {@link
     * TabGroupModelFilter} that tabs within a group must be contiguous.
     *
     * <p>Valid order:
     *
     * <ul>
     *   <li>Tab 1, Group A
     *   <li>Tab 2, Group A
     *   <li>Tab 3, Group B
     * </ul>
     *
     * <p>Invalid order:
     *
     * <ul>
     *   <li>Tab 1, Group A
     *   <li>Tab 2, Group B
     *   <li>Tab 3, Group A
     * </ul>
     */
    @VisibleForTesting
    public boolean isOrderValid() {
        HashSet<Integer> processedRootIds = new HashSet<>();
        int lastRootId = Tab.INVALID_TAB_ID;
        // Iterate over tab model and check that all tabs with the same rootId are next to one
        // another. If at any time a rootId is repeated without the prior tab having the same rootId
        // then the invariant is violated.
        for (int i = 0; i < getTabModel().getCount(); i++) {
            int rootId = getTabModel().getTabAt(i).getRootId();
            if (rootId == lastRootId) continue;

            if (processedRootIds.contains(rootId)) return false;

            processedRootIds.add(lastRootId);
            lastRootId = rootId;
        }
        return true;
    }

    /**
     * Fixes root identifiers to guarantee a group's root identifier is the tab id of one of its
     * tabs.
     *
     * @return the number of groups that had to be fixed.
     */
    @VisibleForTesting
    public int fixRootIds() {
        int fixedRootIdCount = 0;
        TabModel model = getTabModel();
        for (Map.Entry<Integer, TabGroup> entry : mRootIdToGroupMap.entrySet()) {
            int rootId = entry.getKey();
            TabGroup group = entry.getValue();
            if (group.contains(rootId)) continue;

            int fixedRootId = group.getTabIdOfFirstTab();
            for (int tabId : group.getTabIdList()) {
                Tab tab = TabModelUtils.getTabById(model, tabId);
                tab.setRootId(fixedRootId);
            }
            fixedRootIdCount++;
        }

        if (fixedRootIdCount != 0) {
            resetFilterState();
        }
        return fixedRootIdCount;
    }

    @Override
    public int getValidPosition(Tab tab, int proposedPosition) {
        int rootId = Tab.INVALID_TAB_ID;
        if (shouldUseParentIds(tab)) {
            Tab parentTab = getParentTab(tab);
            if (parentTab != null) {
                rootId = parentTab.getRootId();
            }
        } else {
            rootId = tab.getRootId();
        }
        int newPosition = proposedPosition;
        // If the tab is not in the model and won't be part of a group ensure it is positioned
        // outside any other groups.
        if (rootId == Tab.INVALID_TAB_ID || !mRootIdToGroupMap.containsKey(rootId)) {
            newPosition = getValidPositionOfUngroupedTab(proposedPosition);
        } else {
            // The tab is or will be part of a group. Ensure it will be positioned with other
            // members of its group.
            TabGroup group = mRootIdToGroupMap.get(rootId);
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
                && getTabModel().getTabAt(moveToIndex - 1).getRootId()
                        == getTabModel().getTabAt(moveToIndex).getRootId()) {
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
        int rootIdBeforeMove = getRootIdBeforeMove(tab, isMergeTabToGroup || isMoveTabOutOfGroup);
        assert rootIdBeforeMove != TabGroup.INVALID_ROOT_ID;
        TabGroup groupBeforeMove = mRootIdToGroupMap.get(rootIdBeforeMove);

        if (isMoveTabOutOfGroup) {
            resetFilterState();

            int prevFilterIndex = mRootIdToGroupIndexMap.get(rootIdBeforeMove);
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(tab, prevFilterIndex);
            }
        } else if (isMergeTabToGroup) {
            resetFilterState();
            if (groupBeforeMove != null && groupBeforeMove.size() != 1) return;

            TabGroup group = mRootIdToGroupMap.get(tab.getRootId());
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(tab, group.getLastShownTabId());
            }
        } else {
            reorder();
            if (isMoveWithinGroup(tab, curIndex, newIndex)) {
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.didMoveWithinGroup(tab, curIndex, newIndex);
                }
            } else {
                if (!hasFinishedMovingGroup(tab, newIndex)) return;
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.didMoveTabGroup(tab, curIndex, newIndex);
                }
            }
        }

        super.didMoveTab(tab, newIndex, curIndex);
    }

    /** Get all tab group root ids that are associated with tab groups greater than size 1. */
    public Set<Integer> getAllTabGroupRootIds() {
        Set<Integer> uniqueTabGroupRootIds = new ArraySet<>();
        TabList tabList = getTabModel();

        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (isTabInTabGroup(tab)) {
                uniqueTabGroupRootIds.add(tab.getRootId());
            }
        }
        return uniqueTabGroupRootIds;
    }

    private boolean isMoveTabOutOfGroup(Tab movedTab) {
        return !mRootIdToGroupMap.containsKey(movedTab.getRootId());
    }

    private boolean isMergeTabToGroup(Tab tab) {
        int rootId = tab.getRootId();
        if (!mRootIdToGroupMap.containsKey(rootId)) return false;
        TabGroup tabGroup = mRootIdToGroupMap.get(rootId);
        return !tabGroup.contains(tab.getId());
    }

    private int getRootIdBeforeMove(Tab tabToMove, boolean isMoveToDifferentGroup) {
        if (!isMoveToDifferentGroup) return tabToMove.getRootId();

        Set<Integer> rootIdSet = mRootIdToGroupMap.keySet();
        for (Integer rootIdKey : rootIdSet) {
            if (mRootIdToGroupMap.get(rootIdKey).contains(tabToMove.getId())) {
                return rootIdKey;
            }
        }

        return TabGroup.INVALID_ROOT_ID;
    }

    private boolean isMoveWithinGroup(
            Tab movedTab, int oldIndexInTabModel, int newIndexInTabModel) {
        int startIndex = Math.min(oldIndexInTabModel, newIndexInTabModel);
        int endIndex = Math.max(oldIndexInTabModel, newIndexInTabModel);
        for (int i = startIndex; i <= endIndex; i++) {
            if (getTabModel().getTabAt(i).getRootId() != movedTab.getRootId()) return false;
        }
        return true;
    }

    private boolean hasFinishedMovingGroup(Tab movedTab, int newIndexInTabModel) {
        TabGroup tabGroup = mRootIdToGroupMap.get(movedTab.getRootId());
        int offsetIndex = newIndexInTabModel - tabGroup.size() + 1;
        if (offsetIndex < 0) return false;

        for (int i = newIndexInTabModel; i >= offsetIndex; i--) {
            if (getTabModel().getTabAt(i).getRootId() != movedTab.getRootId()) return false;
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
        return mRootIdToGroupMap.size();
    }

    @Override
    public Tab getTabAt(int index) {
        if (index < 0 || index >= getCount()) return null;
        int rootId = Tab.INVALID_TAB_ID;
        Set<Integer> rootIdSet = mRootIdToGroupIndexMap.keySet();
        for (Integer rootIdKey : rootIdSet) {
            if (mRootIdToGroupIndexMap.get(rootIdKey) == index) {
                rootId = rootIdKey;
                break;
            }
        }
        if (rootId == Tab.INVALID_TAB_ID) return null;

        return TabModelUtils.getTabById(
                getTabModel(), mRootIdToGroupMap.get(rootId).getLastShownTabId());
    }

    @Override
    public int indexOf(Tab tab) {
        if (tab == null
                || tab.isIncognito() != isIncognito()
                || getTabModel().indexOf(tab) == TabList.INVALID_TAB_INDEX) {
            return TabList.INVALID_TAB_INDEX;
        }

        int rootId = tab.getRootId();
        if (!mRootIdToGroupIndexMap.containsKey(rootId)) return TabList.INVALID_TAB_INDEX;
        return mRootIdToGroupIndexMap.get(rootId);
    }

    /**
     * @param rootId The rootId of the group to lookup.
     * @return the last shown tab in that group or Tab.INVALID_TAB_ID otherwise.
     */
    public int getGroupLastShownTabId(int rootId) {
        TabGroup group = mRootIdToGroupMap.get(rootId);
        return group == null ? Tab.INVALID_TAB_ID : group.getLastShownTabId();
    }

    /**
     * @param rootId The rootId of the group to lookup.
     * @return the last shown tab in that group or null otherwise.
     */
    public @Nullable Tab getGroupLastShownTab(int rootId) {
        TabGroup group = mRootIdToGroupMap.get(rootId);
        if (group == null) return null;

        int lastShownId = group.getLastShownTabId();
        if (lastShownId == Tab.INVALID_TAB_ID) return null;

        return TabModelUtils.getTabById(getTabModel(), lastShownId);
    }

    /**
     * @param rootId The root identifier of the tab group.
     * @return Whether the given rootId has any tab group associated with it.
     */
    public boolean tabGroupExistsForRootId(int rootId) {
        TabGroup group = mRootIdToGroupMap.get(rootId);
        return group != null;
    }

    /** Returns the current title of the tab group. */
    public String getTabGroupTitle(int rootId) {
        return TabGroupTitleUtils.getTabGroupTitle(rootId);
    }

    /** Stores the given title for the tab group. */
    public void setTabGroupTitle(int rootId, String title) {
        TabGroupTitleUtils.storeTabGroupTitle(rootId, title);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupTitle(rootId, title);
        }
    }

    /** Returns the current color of the tab group. */
    public @TabGroupColorId int getTabGroupColor(int rootId) {
        // TODO(crbug.com/329127327): Refactor and emit an event when this changes the color.
        return TabGroupColorUtils.getOrCreateTabGroupColor(rootId, this);
    }

    /** Stores the given color for the tab group. */
    public void setTabGroupColor(int rootId, @TabGroupColorId int color) {
        TabGroupColorUtils.storeTabGroupColor(rootId, color);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupColor(rootId, color);
        }
    }

    private static Token getOrCreateTabGroupId(@NonNull Tab tab) {
        return getOrCreateTabGroupIdWithDefault(tab, null);
    }

    private static Token getOrCreateTabGroupIdWithDefault(
            @NonNull Tab tab, @Nullable Token defaultTabGroupId) {
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null && ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()) {
            tabGroupId = (defaultTabGroupId == null) ? Token.createRandom() : defaultTabGroupId;
            tab.setTabGroupId(tabGroupId);
        }
        return tabGroupId;
    }
}
