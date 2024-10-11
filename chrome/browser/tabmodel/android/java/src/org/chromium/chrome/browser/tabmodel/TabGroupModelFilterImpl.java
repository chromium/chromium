// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * An implementation of {@link TabGroupModelFilterInternal} that puts {@link Tab}s into a group
 * structure.
 *
 * <p>A group is a collection of {@link Tab}s that share a common ancestor {@link Tab}. This filter
 * is also a {@link TabList} that contains the last shown {@link Tab} from every group.
 *
 * <p>Note this class is in the process of migrating from root ID to TabGroupId. All references to
 * root ID refer to the old ID system. References to tab group ID will refer to the new system. See
 * https://crbug.com/1523745. Update July 2024: the flag for the new TabGroupId system has been
 * removed and it is now launched. This class (and any clients) still need to be migrated off of
 * root ID.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class TabGroupModelFilterImpl implements TabGroupModelFilterInternal, TabModelObserver {
    private static final List<Tab> sEmptyRelatedTabList =
            Collections.unmodifiableList(new ArrayList<Tab>());
    private static final List<Integer> sEmptyRelatedTabIds =
            Collections.unmodifiableList(new ArrayList<Integer>());

    /**
     * Class to hold metadata while fixRootIds still exists. Delete when rootId is removed.
     * Instanced to allow easy setting of fields in constructor.
     */
    private class TabGroupMetadata {
        public final String title;
        public final int color;
        public final boolean isCollapsed;

        public TabGroupMetadata(int rootId) {
            title = getTabGroupTitle(rootId);
            color = getTabGroupColorWithFallback(rootId);
            isCollapsed = getTabGroupCollapsed(rootId);
        }
    }

    private final TabModel mTabModel;
    private final ObserverList<TabModelObserver> mFilteredObservers = new ObserverList<>();
    private final ObserverList<TabGroupModelFilterObserver> mGroupFilterObserver =
            new ObserverList<>();
    private final Map<Integer, Integer> mRootIdToGroupIndexMap = new HashMap<>();
    private final Map<Integer, TabGroup> mRootIdToGroupMap = new HashMap<>();

    /**
     * The set of tab group IDs that are currently hiding. This cannot be stored on {@link TabGroup}
     * as for undoable closures that object will already be gone before tab closures are finished.
     */
    private Set<Token> mHidingTabGroups = new HashSet<>();

    private int mCurrentGroupIndex = TabList.INVALID_TAB_INDEX;
    private Tab mAbsentSelectedTab;
    private boolean mShouldRecordUma = true;
    private boolean mTabRestoreCompleted;
    private boolean mTabStateInitialized;
    private boolean mIsResetting;
    private boolean mIsUndoing;

    /**
     * @param tabModel The tab model to filter.
     */
    public TabGroupModelFilterImpl(TabModel tabModel) {
        mTabModel = tabModel;
        mTabModel.addObserver(this);
    }

    @Override
    public void destroy() {
        mFilteredObservers.clear();
        mTabModel.removeObserver(this);
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mFilteredObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mFilteredObservers.removeObserver(observer);
    }

    @Override
    public void addTabGroupObserver(TabGroupModelFilterObserver observer) {
        mGroupFilterObserver.addObserver(observer);
    }

    @Override
    public void removeTabGroupObserver(TabGroupModelFilterObserver observer) {
        mGroupFilterObserver.removeObserver(observer);
    }

    @Override
    public boolean isCurrentlySelectedFilter() {
        return getTabModel().isActiveModel();
    }

    @Override
    public @NonNull TabModel getTabModel() {
        return mTabModel;
    }

    @Override
    public boolean isTabModelRestored() {
        // TODO(crbug.com/40130477): Remove |mTabRestoreCompleted|. |mTabRestoreCompleted| is always
        // false for incognito, while |mTabStateInitialized| is not. |mTabStateInitialized| is
        // marked after the TabModelSelector is initialized, therefore it is the true state of the
        // TabModel.
        return mTabRestoreCompleted || mTabStateInitialized;
    }

    @Override
    public int getTotalTabCount() {
        return mTabModel.getCount();
    }

    @Override
    public int getTabGroupCount() {
        if (!isTabModelRestored() || mIsResetting) return -1;

        TabModel model = getTabModel();
        int count = 0;
        for (TabGroup group : mRootIdToGroupMap.values()) {
            if (isTabInTabGroup(model.getTabById(group.getLastShownTabId()))) {
                count++;
            }
        }
        return count;
    }

    @Override
    public int getIndexOfTabInGroup(Tab tab) {
        TabGroup tabGroup = mRootIdToGroupMap.get(tab.getRootId());
        if (tabGroup == null) return TabGroup.INVALID_POSITION_IN_GROUP;
        return tabGroup.getPositionOfTab(tab);
    }

    @Override
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

    @Override
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge) {
        for (Tab tab : tabsToMerge) {
            if (isTabInTabGroup(tab)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void createSingleTabGroup(int tabId, boolean notify) {
        createSingleTabGroup(getTabModel().getTabById(tabId), notify);
    }

    @Override
    public void createSingleTabGroup(Tab tab, boolean notify) {
        assert tab.getTabGroupId() == null;

        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.willMergeTabToGroup(tab, tab.getRootId());
        }

        tab.setTabGroupId(Token.createRandom());

        // If this is a new tab group creation that will show a dialog, do not trigger a snackbar.
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()
                && !shouldSkipGroupCreationDialog(
                        TabGroupFeatureUtils.shouldShowGroupCreationDialogViaSettingsSwitch())) {
            notify = false;
        }

        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didCreateNewGroup(tab, this);
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
                        TabGroupColorUtils.INVALID_COLOR_ID,
                        /* destinationGroupTitleCollapsed= */ false);
            }
        }
    }

    @Override
    public void mergeTabsToGroup(int sourceTabId, int destinationTabId) {
        mergeTabsToGroup(sourceTabId, destinationTabId, false);
    }

    @Override
    public void mergeTabsToGroup(
            int sourceTabId, int destinationTabId, boolean skipUpdateTabModel) {
        Tab sourceTab = getTabModel().getTabById(sourceTabId);
        Tab destinationTab = getTabModel().getTabById(destinationTabId);

        assert sourceTab != null
                        && destinationTab != null
                        && sourceTab.isIncognito() == destinationTab.isIncognito()
                : "Attempting to merge groups from different model";

        List<Tab> tabsToMerge = getRelatedTabList(sourceTabId);
        int destinationIndexInTabModel = getTabModelDestinationIndex(destinationTab);

        if (!skipUpdateTabModel && needToUpdateTabModel(tabsToMerge, destinationIndexInTabModel)) {
            mergeListOfTabsToGroup(tabsToMerge, destinationTab, !skipUpdateTabModel);
        } else {
            int destinationRootId = destinationTab.getRootId();
            List<Tab> tabsIncludingDestination = new ArrayList<>();
            List<Integer> originalIndexes = new ArrayList<>();
            List<Integer> originalRootIds = new ArrayList<>();
            List<Token> originalTabGroupIds = new ArrayList<>();
            Set<Pair<Integer, Token>> removedGroups = new HashSet<>();
            String destinationGroupTitle = TabGroupTitleUtils.getTabGroupTitle(destinationRootId);
            int destinationGroupColorId = TabGroupColorUtils.INVALID_COLOR_ID;
            boolean willMergingCreateNewGroup =
                    willMergingCreateNewGroup(List.of(sourceTab, destinationTab));

            if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                destinationGroupColorId = TabGroupColorUtils.getTabGroupColor(destinationRootId);
            }

            final boolean destinationGroupTitleCollapsed;
            if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
                destinationGroupTitleCollapsed = getTabGroupCollapsed(destinationRootId);
            } else {
                destinationGroupTitleCollapsed = false;
            }

            if (!skipUpdateTabModel) {
                originalIndexes.add(
                        TabModelUtils.getTabIndexById(getTabModel(), destinationTab.getId()));
                originalTabGroupIds.add(destinationTab.getTabGroupId());
            }
            tabsIncludingDestination.add(destinationTab);
            originalRootIds.add(destinationRootId);

            Token destinationTabGroupId =
                    getOrCreateTabGroupIdWithDefault(destinationTab, sourceTab.getTabGroupId());

            for (int i = 0; i < tabsToMerge.size(); i++) {
                Tab tab = tabsToMerge.get(i);
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.willMergeTabToGroup(tab, destinationRootId);
                }

                // Skip unnecessary work of populating the lists if logic is skipped below.
                if (!skipUpdateTabModel) {
                    int index = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
                    assert index != TabModel.INVALID_TAB_INDEX;
                    originalIndexes.add(index);
                    originalTabGroupIds.add(tab.getTabGroupId());
                }
                tabsIncludingDestination.add(tab);
                originalRootIds.add(tab.getRootId());
                @Nullable Token tabGroupId = tab.getTabGroupId();
                if (tabGroupId != null) {
                    @Nullable
                    Token oldTabGroupId =
                            tabGroupId.equals(destinationTabGroupId) ? null : tabGroupId;
                    removedGroups.add(Pair.create(tab.getRootId(), oldTabGroupId));
                }

                setBothGroupIds(tab, destinationRootId, destinationTabGroupId);
            }
            resetFilterState();

            Tab lastMergedTab = tabsToMerge.get(tabsToMerge.size() - 1);
            TabGroup group = mRootIdToGroupMap.get(lastMergedTab.getRootId());
            for (int i = 0; i < tabsToMerge.size(); i++) {
                Tab tab = tabsToMerge.get(i);
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.didMergeTabToGroup(tab, group.getLastShownTabId());
                }
            }

            // TODO(b/339480989): Resequence this so that we iterate over observers multiple times
            // and emit one event per loop to be consistent with other usages.
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                if (willMergingCreateNewGroup) {
                    observer.didCreateNewGroup(destinationTab, this);

                    // If this is a new tab group creation that will show a dialog, do not trigger a
                    // snackbar.
                    if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()
                            && !shouldSkipGroupCreationDialog(
                                    TabGroupFeatureUtils
                                            .shouldShowGroupCreationDialogViaSettingsSwitch())) {
                        continue;
                    }
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
                            destinationGroupColorId,
                            destinationGroupTitleCollapsed);
                } else {
                    for (int i = 0; i < tabsIncludingDestination.size(); i++) {
                        Tab tab = tabsIncludingDestination.get(i);
                        int rootId = originalRootIds.get(i);
                        if (tab.getRootId() == rootId) continue;
                        deleteTabGroupVisualData(rootId);
                    }
                }

                for (Pair<Integer, Token> removedGroup : removedGroups) {
                    observer.didRemoveTabGroup(
                            removedGroup.first, removedGroup.second, DidRemoveTabGroupReason.MERGE);
                }
            }
        }
    }

    @Override
    public void mergeListOfTabsToGroup(List<Tab> tabs, Tab destinationTab, boolean notify) {
        // Check whether the destination tab is in a tab group before getOrCreateTabGroupId so we
        // send the correct signal for whether a tab group was newly created.
        List<Tab> tabsToMerge = new ArrayList<>();
        tabsToMerge.addAll(tabs);
        tabsToMerge.add(destinationTab);
        boolean willMergingCreateNewGroup = willMergingCreateNewGroup(tabsToMerge);

        List<Tab> mergedTabs = new ArrayList<>();
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();
        List<Token> originalTabGroupIds = new ArrayList<>();
        Set<Pair<Integer, Token>> removedGroups = new HashSet<>();

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
        int destinationGroupColorId = TabGroupColorUtils.INVALID_COLOR_ID;
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            destinationGroupColorId = TabGroupColorUtils.getTabGroupColor(destinationRootId);
        }

        final boolean destinationGroupTitleCollapsed;
        if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
            destinationGroupTitleCollapsed = getTabGroupCollapsed(destinationRootId);
        } else {
            destinationGroupTitleCollapsed = false;
        }

        // Iterate through all tabs to set the proper new group creation status.
        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);

            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.willMergeTabToGroup(tab, destinationRootId);
            }

            if (tab.getId() == destinationTab.getId()) continue;

            int index = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
            assert index != TabModel.INVALID_TAB_INDEX;

            mergedTabs.add(tab);
            originalIndexes.add(index);
            originalRootIds.add(tab.getRootId());
            originalTabGroupIds.add(tab.getTabGroupId());
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null) {
                @Nullable
                Token oldTabGroupId = tabGroupId.equals(destinationTabGroupId) ? null : tabGroupId;
                removedGroups.add(Pair.create(tab.getRootId(), oldTabGroupId));
            }
            boolean isMergingBackward = index < destinationIndexInTabModel;

            setBothGroupIds(tab, destinationRootId, destinationTabGroupId);
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
            if (willMergingCreateNewGroup) {
                observer.didCreateNewGroup(destinationTab, this);
            }

            // If this is a new tab group creation that will show a dialog, do not trigger a
            // snackbar.
            boolean skipSnackbarForCreation =
                    willMergingCreateNewGroup
                            && ChromeFeatureList.sTabGroupParityAndroid.isEnabled()
                            && !shouldSkipGroupCreationDialog(
                                    TabGroupFeatureUtils
                                            .shouldShowGroupCreationDialogViaSettingsSwitch());
            if (notify && !skipSnackbarForCreation) {
                observer.didCreateGroup(
                        mergedTabs,
                        originalIndexes,
                        originalRootIds,
                        originalTabGroupIds,
                        destinationGroupTitle,
                        destinationGroupColorId,
                        destinationGroupTitleCollapsed);
            } else {
                for (int i = 0; i < mergedTabs.size(); i++) {
                    Tab tab = mergedTabs.get(i);
                    int rootId = originalRootIds.get(i);
                    if (tab.getRootId() == rootId) continue;
                    deleteTabGroupVisualData(rootId);
                }
            }

            for (Pair<Integer, Token> removedGroup : removedGroups) {
                observer.didRemoveTabGroup(
                        removedGroup.first, removedGroup.second, DidRemoveTabGroupReason.MERGE);
            }
        }
    }

    @Override
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing) {
        TabModel tabModel = getTabModel();
        Tab sourceTab = tabModel.getTabById(sourceTabId);
        int sourceIndex = tabModel.indexOf(sourceTab);
        int oldRootId = sourceTab.getRootId();
        TabGroup sourceTabGroup = mRootIdToGroupMap.get(oldRootId);

        if (sourceTabGroup.size() == 1) {
            int prevFilterIndex = mRootIdToGroupIndexMap.get(oldRootId);
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.willMoveTabOutOfGroup(sourceTab, oldRootId);
            }
            // When moving the last tab out of a tab group of size 1 we should decrement the number
            // of tab groups.
            if (sourceTab.getTabGroupId() != null) {
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.didRemoveTabGroup(
                            oldRootId, sourceTab.getTabGroupId(), DidRemoveTabGroupReason.UNGROUP);
                }
            }
            sourceTab.setTabGroupId(null);
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(sourceTab, prevFilterIndex);
            }
            return;
        }

        int targetIndex;
        if (trailing) {
            Tab lastTabInSourceGroup = tabModel.getTabById(sourceTabGroup.getTabIdOfLastTab());
            targetIndex = tabModel.indexOf(lastTabInSourceGroup);
        } else {
            Tab firstTabInSourceGroup = tabModel.getTabById(sourceTabGroup.getTabIdOfFirstTab());
            targetIndex = tabModel.indexOf(firstTabInSourceGroup);
        }
        assert targetIndex != TabModel.INVALID_TAB_INDEX;

        boolean sourceTabIdWasRootId = sourceTab.getId() == oldRootId;
        int newRootId = oldRootId;
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

        TabStateAttributes tabStateAttributes = TabStateAttributes.from(sourceTab);
        tabStateAttributes.beginBatchEdit();
        sourceTab.setTabGroupId(null);
        if (sourceTabIdWasRootId) {
            for (int tabId : sourceTabGroup.getTabIdList()) {
                Tab tab = tabModel.getTabById(tabId);
                // One of these iterations will update the rootId of the moved tab. This seems
                // surprising/unnecessary, but is actually critical for #isMoveTabOutOfGroup to work
                // correctly.
                tab.setRootId(newRootId);
            }
            resetFilterState();
        }
        sourceTab.setRootId(sourceTab.getId());
        tabStateAttributes.endBatchEdit();

        if (sourceTabIdWasRootId) {
            // Must be done here instead of lower down, as the GTS currently does not listen to
            // metadata changes that will trigger from this, and will otherwise poll group data too
            // early.
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didChangeGroupRootId(oldRootId, newRootId);
            }
        }

        // If moving tab is already in the target index in tab model, no move in tab model.
        if (sourceIndex == targetIndex) {
            resetFilterState();
            // Find the group that the tab was in that now has newRootId.
            int prevFilterIndex = mRootIdToGroupIndexMap.get(newRootId);
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(sourceTab, prevFilterIndex);
            }
        } else {
            // Plus one as offset because we are moving backwards in tab model.
            tabModel.moveTab(sourceTab.getId(), trailing ? targetIndex + 1 : targetIndex);
        }
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

    @Override
    public void undoGroupedTab(
            Tab tab, int originalIndex, int originalRootId, @Nullable Token originalTabGroupId) {
        if (!tab.isInitialized()) return;

        int currentIndex = TabModelUtils.getTabIndexById(getTabModel(), tab.getId());
        assert currentIndex != TabModel.INVALID_TAB_INDEX;

        boolean isChangingRootIds = tab.getRootId() != originalRootId;
        boolean isChangingStableIds = !Objects.equals(tab.getTabGroupId(), originalTabGroupId);
        boolean isChangingGroups = isChangingRootIds || isChangingStableIds;
        boolean isChangingIndex = currentIndex != originalIndex;

        // We need to explicitly trigger `didMoveTabOutOfGroup` if the tab is changing groups so
        // that the old group is aware the tab is no longer in the group. The detection logic in
        // `didMoveTab` fails to correctly handle this case if the tab is moving between tab
        // groups because it lacks enough context, hence the `mIsUndoing` variable is used as a
        // bodge to communicate this. Then we signal `didMergeTabToGroup` separately afterwards
        // so long as the tab is actually becoming part of a tab group.
        mIsUndoing = isChangingGroups;

        setBothGroupIds(tab, originalRootId, originalTabGroupId);
        if (isChangingIndex) {
            if (currentIndex < originalIndex) originalIndex++;
            getTabModel().moveTab(tab.getId(), originalIndex);
        } else if (isChangingGroups) {
            didMoveTab(tab, originalIndex, currentIndex);
        }
        // Else we can ignore tabs that remain at the same index if they are not changing root IDs.

        mIsUndoing = false;

        // If undoing results in restoring a tab into a different group then notify observers it was
        // added.
        // TODO(b/b/339480464): Emit a matching willMergeTabToGroup somewhere upstream.
        if (isChangingGroups && isTabInTabGroup(tab)) {
            TabGroup group = mRootIdToGroupMap.get(originalRootId);
            // Last shown tab IDs are not preserved across an undo.
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMergeTabToGroup(tab, group.getLastShownTabId());
            }
        }
    }

    @NonNull
    @Override
    public List<Tab> getRelatedTabList(int id) {
        Tab tab = getTabModel().getTabById(id);
        if (tab == null) return sEmptyRelatedTabList;

        int rootId = tab.getRootId();
        TabGroup group = mRootIdToGroupMap.get(rootId);
        if (group == null) return sEmptyRelatedTabList;
        return getRelatedTabList(group.getTabIdList());
    }

    @Override
    public List<Integer> getRelatedTabIds(int tabId) {
        Tab tab = getTabModel().getTabById(tabId);
        if (tab == null) return sEmptyRelatedTabIds;

        int rootId = tab.getRootId();
        TabGroup group = mRootIdToGroupMap.get(rootId);
        if (group == null) return sEmptyRelatedTabIds;
        return Collections.unmodifiableList(group.getTabIdList());
    }

    @Override
    public List<Tab> getRelatedTabListForRootId(int tabRootId) {
        if (tabRootId == Tab.INVALID_TAB_ID) return sEmptyRelatedTabList;
        TabGroup group = mRootIdToGroupMap.get(tabRootId);
        if (group == null) return sEmptyRelatedTabList;
        return getRelatedTabList(group.getTabIdList());
    }

    @Override
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
        return isInGroup && tab.getTabGroupId() != null;
    }

    private List<Tab> getRelatedTabList(List<Integer> ids) {
        List<Tab> tabs = new ArrayList<>();
        for (Integer id : ids) {
            Tab tab = getTabModel().getTabById(id);
            // TODO(crbug.com/40245624): If this is called during a TabModelObserver observer
            // iterator it is possible a sequencing issue can occur where the tab is gone from the
            // TabModel, but still exists in the TabGroup. Avoid returning null by skipping the tab
            // if it doesn't exist in the TabModel.
            if (tab == null) continue;
            tabs.add(tab);
        }
        return Collections.unmodifiableList(tabs);
    }

    private boolean shouldUseParentIds(Tab tab) {
        @TabLaunchType int tabLaunchType = tab.getLaunchType();
        return isTabModelRestored()
                && !mIsResetting
                && (tabLaunchType == TabLaunchType.FROM_TAB_GROUP_UI
                        || tabLaunchType == TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP
                        || tabLaunchType == TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP);
    }

    private Tab getParentTab(Tab tab) {
        return getTabModel().getTabById(tab.getParentId());
    }

    @VisibleForTesting
    void addTab(Tab tab, boolean fromUndo) {
        if (tab.isIncognito() != isIncognito()) {
            throw new IllegalStateException("Attempting to open tab in the wrong model");
        }

        boolean willMergingCreateNewGroup = false;
        if (!fromUndo && shouldUseParentIds(tab)) {
            Tab parentTab = getParentTab(tab);
            if (parentTab != null) {
                Token oldTabGroupId = parentTab.getTabGroupId();
                Token newTabGroupId = getOrCreateTabGroupId(parentTab);
                if (!Objects.equals(oldTabGroupId, newTabGroupId)) {
                    willMergingCreateNewGroup = true;
                }
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.willMergeTabToGroup(tab, parentTab.getRootId());
                }
                tab.setRootId(parentTab.getRootId());
                tab.setTabGroupId(newTabGroupId);
            }
        }

        int rootId = tab.getRootId();
        if (mRootIdToGroupMap.containsKey(rootId)) {
            mRootIdToGroupMap.get(rootId).addTab(tab.getId(), getTabModel());

            if (willMergingCreateNewGroup) {
                // TODO(crbug.com/40173284): Update UMA for Context menu creation.
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
            TabGroup tabGroup = new TabGroup();
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

    @VisibleForTesting
    void closeTab(Tab tab) {
        int rootId = tab.getRootId();
        if (tab.isIncognito() != isIncognito()
                || mRootIdToGroupMap.get(rootId) == null
                || !mRootIdToGroupMap.get(rootId).contains(tab.getId())) {
            throw new IllegalStateException("Attempting to close tab in the wrong model");
        }

        TabGroup group = mRootIdToGroupMap.get(rootId);
        group.removeTab(tab.getId());

        // If the removed tab's id was the root id, we need to select a new root id.
        if (tab.getRootId() == tab.getId()) {
            int nextRootId = group.getLastShownTabId();
            if (nextRootId != INVALID_TAB_INDEX && nextRootId != rootId) {
                // Use the comprehensive model to ensure undoable closures that happened before our
                // current closure also get updated, so they'll restore into the same group.
                TabList comprehensiveModel = getTabModel().getComprehensiveModel();
                int comprehensiveCount = comprehensiveModel.getCount();
                for (int i = 0; i < comprehensiveCount; ++i) {
                    Tab comprehensiveTab = comprehensiveModel.getTabAt(i);
                    if (comprehensiveTab.getRootId() == rootId) {
                        comprehensiveTab.setRootId(nextRootId);
                    }
                }
                mRootIdToGroupIndexMap.put(nextRootId, mRootIdToGroupIndexMap.remove(rootId));
                mRootIdToGroupMap.put(nextRootId, mRootIdToGroupMap.remove(rootId));
                for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                    observer.didChangeGroupRootId(rootId, nextRootId);
                }
            }
        }

        boolean didRemoveGroup = false;
        if (group.size() == 0) {
            didRemoveGroup = true;
        }
        if (group.size() == 0) {
            updateRootIdToGroupIndexMapAfterGroupClosed(rootId);
            mRootIdToGroupIndexMap.remove(rootId);
            mRootIdToGroupMap.remove(rootId);
        }

        if (didRemoveGroup) {
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didRemoveTabGroup(
                        rootId, tab.getTabGroupId(), DidRemoveTabGroupReason.CLOSE);
            }
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

    @VisibleForTesting
    void selectTab(Tab tab) {
        assert mAbsentSelectedTab == null;

        if (tab == null) return;

        int rootId = tab.getRootId();
        if (mRootIdToGroupMap.get(rootId) == null) {
            mAbsentSelectedTab = tab;
        } else {
            mRootIdToGroupMap.get(rootId).setLastShownTabId(tab.getId());
            mCurrentGroupIndex = mRootIdToGroupIndexMap.get(rootId);
        }
    }

    private void reorder() {
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

    @VisibleForTesting
    void removeTab(Tab tab) {
        closeTab(tab);
    }

    /**
     * Clean up filter internal data, and resets the internal data based on the current {@link
     * TabModel}.
     */
    @VisibleForTesting
    public void resetFilterState() {
        mShouldRecordUma = false;
        mIsResetting = true;
        Map<Integer, Integer> rootIdToGroupLastShownTabId = new HashMap<>();
        for (int rootId : mRootIdToGroupMap.keySet()) {
            rootIdToGroupLastShownTabId.put(
                    rootId, mRootIdToGroupMap.get(rootId).getLastShownTabId());
        }

        mRootIdToGroupIndexMap.clear();
        mRootIdToGroupMap.clear();
        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            addTab(tab, /* fromUndo= */ false);
        }

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
        if (tabModel.index() == TabModel.INVALID_TAB_INDEX) {
            mCurrentGroupIndex = TabModel.INVALID_TAB_INDEX;
        } else {
            selectTab(tabModel.getTabAt(tabModel.index()));
        }
        mShouldRecordUma = true;
        mIsResetting = false;
    }

    // TODO(crbug.com/41450619): This is a band-aid fix for not crashing when undo the last closed
    // tab, should remove later.
    /** Returns whether filter should notify observers about the SetIndex call. */
    private boolean shouldNotifyObserversOnSetIndex() {
        return mAbsentSelectedTab == null;
    }

    @Override
    public void markTabStateInitialized() {
        assert !mTabStateInitialized;
        mTabStateInitialized = true;
        boolean correctOrder = isOrderValid();
        RecordHistogram.recordBooleanHistogram("Tabs.Tasks.OrderValidOnStartup", correctOrder);

        int fixedRootIdCount = fixRootIds();
        RecordHistogram.recordCount1000Histogram(
                "TabGroups.NumberOfRootIdsFixed", fixedRootIdCount);

        // There's an assertion being hit where archived/restored tabs are being counted as part of
        // a tab group. See crbug.com/356330532 for more details.
        resetFilterState();
        addTabGroupIdsForAllTabGroups();
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
                assert lastTabGroupId != null
                        : String.format(
                                Locale.getDefault(),
                                "Expected tab group id for matching root id=%d, tab index=%d, tab"
                                        + " count=%d",
                                rootId,
                                i,
                                model.getCount());
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

        // Cannot simply move metadata when we change root ids. It's possible what used to be one
        // group has become two groups, in which case we try to copy the metadata into both ids.
        // It's possible that we shift around multiple chains of root ids, even in a circle. To get
        // around these edge cases, hold all metadata in memory, and only start writing metadata
        // after reading everything. Then do a second pass to remove any old metadata that no longer
        // has matching root ids.
        // Note: It was fairly arbitrarily chosen that when we split a group, we duplicate metadata.
        // If we have a reason to change this behavior, we can.
        Map<Integer, Integer> oldToNewRootIds = new HashMap<>();
        Map<Integer, TabGroupMetadata> oldRootIdsToMetadata = new HashMap<>();

        for (Map.Entry<Integer, TabGroup> entry : mRootIdToGroupMap.entrySet()) {
            int rootId = entry.getKey();
            TabGroup group = entry.getValue();
            if (group.contains(rootId)) continue;

            int fixedRootId = group.getTabIdOfFirstTab();
            for (int tabId : group.getTabIdList()) {
                Tab tab = model.getTabById(tabId);
                tab.setRootId(fixedRootId);
            }

            oldRootIdsToMetadata.put(rootId, new TabGroupMetadata(rootId));
            oldToNewRootIds.put(rootId, fixedRootId);
            fixedRootIdCount++;
        }

        if (fixedRootIdCount != 0) {
            for (Entry<Integer, Integer> oldToNew : oldToNewRootIds.entrySet()) {
                int oldRootId = oldToNew.getKey();
                int newRootId = oldToNew.getValue();
                TabGroupMetadata metadata = oldRootIdsToMetadata.get(oldRootId);
                if (metadata.title != null) setTabGroupTitle(newRootId, metadata.title);
                if (metadata.color != TabGroupColorUtils.INVALID_COLOR_ID) {
                    setTabGroupColor(newRootId, metadata.color);
                }
                if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
                    if (metadata.isCollapsed) setTabGroupCollapsed(newRootId, true);
                }
            }

            resetFilterState();

            for (Entry<Integer, Integer> oldToNew : oldToNewRootIds.entrySet()) {
                int oldRootId = oldToNew.getKey();
                if (!mRootIdToGroupMap.containsKey(oldRootId)) {
                    TabGroupTitleUtils.deleteTabGroupTitle(oldRootId);
                    TabGroupColorUtils.deleteTabGroupColor(oldRootId);
                    if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
                        deleteTabGroupCollapsed(oldRootId);
                    }
                }
            }
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
     *
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
     *
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

        if (isMoveTabOutOfGroup) {
            resetFilterState();

            int prevFilterIndex = mRootIdToGroupIndexMap.get(rootIdBeforeMove);
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.didMoveTabOutOfGroup(tab, prevFilterIndex);
            }
        } else if (isMergeTabToGroup) {
            resetFilterState();

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

        for (TabModelObserver observer : mFilteredObservers) {
            observer.didMoveTab(tab, newIndex, curIndex);
        }
    }

    @Override
    public Set<Integer> getAllTabGroupRootIds() {
        Set<Integer> uniqueTabGroupRootIds = new ArraySet<>();
        forEachTabInTabGroup((tab) -> uniqueTabGroupRootIds.add(tab.getRootId()));
        return uniqueTabGroupRootIds;
    }

    @Override
    public Set<Token> getAllTabGroupIds() {
        Set<Token> uniqueTabGroupIds = new ArraySet<>();
        forEachTabInTabGroup((tab) -> uniqueTabGroupIds.add(tab.getTabGroupId()));
        return uniqueTabGroupIds;
    }

    private void forEachTabInTabGroup(Callback<Tab> callback) {
        TabList tabList = getTabModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (isTabInTabGroup(tab)) {
                callback.onResult(tab);
            }
        }
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
    public boolean isOffTheRecord() {
        return getTabModel().isOffTheRecord();
    }

    @Override
    public boolean isIncognitoBranded() {
        return getTabModel().isIncognitoBranded();
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

        return getTabModel().getTabById(mRootIdToGroupMap.get(rootId).getLastShownTabId());
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

    @Override
    public int getGroupLastShownTabId(int rootId) {
        TabGroup group = mRootIdToGroupMap.get(rootId);
        return group == null ? Tab.INVALID_TAB_ID : group.getLastShownTabId();
    }

    @Override
    public @Nullable Tab getGroupLastShownTab(int rootId) {
        TabGroup group = mRootIdToGroupMap.get(rootId);
        if (group == null) return null;

        int lastShownId = group.getLastShownTabId();
        if (lastShownId == Tab.INVALID_TAB_ID) return null;

        return getTabModel().getTabById(lastShownId);
    }

    @Override
    public boolean tabGroupExistsForRootId(int rootId) {
        TabGroup group = mRootIdToGroupMap.get(rootId);
        return group != null;
    }

    @Override
    public String getTabGroupTitle(int rootId) {
        return TabGroupTitleUtils.getTabGroupTitle(rootId);
    }

    @Override
    public void setTabGroupTitle(int rootId, String title) {
        TabGroupTitleUtils.storeTabGroupTitle(rootId, title);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupTitle(rootId, title);
        }
    }

    @Override
    public void deleteTabGroupTitle(int rootId) {
        TabGroupTitleUtils.deleteTabGroupTitle(rootId);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupTitle(rootId, null);
        }
    }

    @Override
    public int getTabGroupColor(int rootId) {
        return TabGroupColorUtils.getTabGroupColor(rootId);
    }

    @Override
    public @TabGroupColorId int getTabGroupColorWithFallback(int rootId) {
        assert rootId != Tab.INVALID_TAB_ID;
        int color = getTabGroupColor(rootId);
        return color == TabGroupColorUtils.INVALID_COLOR_ID ? TabGroupColorId.GREY : color;
    }

    @Override
    public void setTabGroupColor(int rootId, @TabGroupColorId int color) {
        TabGroupColorUtils.storeTabGroupColor(rootId, color);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupColor(rootId, color);
        }
    }

    @Override
    public void deleteTabGroupColor(int rootId) {
        TabGroupColorUtils.deleteTabGroupColor(rootId);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupColor(rootId, TabGroupColorId.GREY);
        }
    }

    @Override
    public void setTabGroupCollapsed(int rootId, boolean isCollapsed) {
        TabGroupCollapsedUtils.storeTabGroupCollapsed(rootId, isCollapsed);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupCollapsed(rootId, isCollapsed);
        }
    }

    @Override
    public void deleteTabGroupCollapsed(int rootId) {
        TabGroupCollapsedUtils.deleteTabGroupCollapsed(rootId);
        for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
            observer.didChangeTabGroupCollapsed(rootId, false);
        }
    }

    @Override
    public boolean getTabGroupCollapsed(int rootId) {
        return TabGroupCollapsedUtils.getTabGroupCollapsed(rootId);
    }

    @Override
    public void deleteTabGroupVisualData(int rootId) {
        deleteTabGroupTitle(rootId);

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            deleteTabGroupColor(rootId);
        }
        if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
            deleteTabGroupCollapsed(rootId);
        }
    }

    @Override
    public String getTabGroupSyncId(int rootId) {
        return TabGroupSyncIdUtils.getTabGroupSyncId(rootId);
    }

    @Override
    public void setTabGroupSyncId(int rootId, String syncId) {
        TabGroupSyncIdUtils.putTabGroupSyncId(rootId, syncId);
    }

    @Override
    public int getRootIdFromStableId(@NonNull Token stableId) {
        for (int i = 0; i < getTabModel().getCount(); i++) {
            Tab tab = getTabModel().getTabAt(i);
            if (stableId.equals(tab.getTabGroupId())) return tab.getRootId();
        }
        return Tab.INVALID_TAB_ID;
    }

    @Override
    public @Nullable Token getStableIdFromRootId(int rootId) {
        TabGroup tabGroup = mRootIdToGroupMap.get(rootId);
        if (tabGroup == null) return null;

        Tab tab = getTabModel().getTabById(tabGroup.getLastShownTabId());
        if (tab == null) return null;

        return tab.getTabGroupId();
    }

    @Override
    public boolean closeTabs(TabClosureParams tabClosureParams) {
        TabModel tabModel = getTabModel();
        if (tabClosureParams.hideTabGroups && canHideTabGroups()) {
            if (tabClosureParams.isAllTabs) {
                for (Token token : getAllTabGroupIds()) {
                    setTabGroupHiding(token);
                }
            } else {
                Set<Integer> closingTabIds =
                        tabClosureParams.tabs.stream().map(Tab::getId).collect(Collectors.toSet());
                for (int rootId : getAllTabGroupRootIds()) {
                    TabGroup group = mRootIdToGroupMap.get(rootId);
                    if (group == null) continue;

                    if (closingTabIds.containsAll(group.getTabIdList())) {
                        Tab tab = tabModel.getTabById(group.getLastShownTabId());
                        setTabGroupHiding(tab.getTabGroupId());
                    }
                }
            }
        }
        return tabModel.closeTabs(tabClosureParams);
    }

    @Override
    public boolean isTabGroupHiding(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return false;

        return mHidingTabGroups.contains(tabGroupId);
    }

    private boolean canHideTabGroups() {
        Profile profile = getTabModel().getProfile();
        if (profile == null || !profile.isNativeInitialized()) return false;

        return !isIncognito() && TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);
    }

    /** Sets that the tab group is hiding rather than being deleted. */
    private void setTabGroupHiding(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return;

        mHidingTabGroups.add(tabGroupId);
    }

    @Override
    public void tabClosureUndone(Tab tab) {
        addTab(tab, /* fromUndo= */ true);
        reorder();
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabClosureUndone(tab);
        }
        @Nullable Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId != null) {
            mHidingTabGroups.remove(tabGroupId);
        }
    }

    @Override
    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.onFinishingMultipleTabClosure(tabs, canRestore);
        }
        Set<Token> processedTabGroups = new HashSet<>();
        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                getLazyAllTabGroupIdsInComprehensiveModel(tabs);
        for (Tab tab : tabs) {
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) continue;

            boolean alreadyProcessed = !processedTabGroups.add(tabGroupId);
            if (alreadyProcessed) continue;

            // If the tab group still exists in the comprehensive tab model then we shouldn't signal
            // that it is finished closing.
            if (tabGroupIdsInComprehensiveModel.get().contains(tabGroupId)) continue;

            boolean wasHiding = mHidingTabGroups.remove(tabGroupId);
            for (TabGroupModelFilterObserver observer : mGroupFilterObserver) {
                observer.committedTabGroupClosure(tabGroupId, wasHiding);
            }
        }
    }

    @Override
    public LazyOneshotSupplier<Set<Token>> getLazyAllTabGroupIdsInComprehensiveModel(
            List<Tab> tabsToExclude) {
        return LazyOneshotSupplier.fromSupplier(
                () -> {
                    Set<Token> tabGroupIds = new HashSet<>();
                    forEachTabInComprehensiveModelExcept(
                            tabsToExclude,
                            tab -> {
                                @Nullable Token tabGroupId = tab.getTabGroupId();
                                if (tabGroupId != null) {
                                    tabGroupIds.add(tabGroupId);
                                }
                            });
                    return tabGroupIds;
                });
    }

    @Override
    public LazyOneshotSupplier<Set<Integer>> getLazyAllRootIdsInComprehensiveModel(
            List<Tab> tabsToExclude) {
        return LazyOneshotSupplier.fromSupplier(
                () -> {
                    Set<Integer> rootIds = new HashSet<>();
                    forEachTabInComprehensiveModelExcept(
                            tabsToExclude,
                            tab -> {
                                rootIds.add(tab.getRootId());
                            });
                    return rootIds;
                });
    }

    private void forEachTabInComprehensiveModelExcept(
            List<Tab> tabsToExclude, Callback<Tab> callback) {
        Set<Tab> tabsToExcludeSet = new HashSet<>(tabsToExclude);
        TabList tabList = getTabModel().getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (tabsToExcludeSet.contains(tab)) continue;

            callback.onResult(tab);
        }
    }

    private static Token getOrCreateTabGroupId(@NonNull Tab tab) {
        return getOrCreateTabGroupIdWithDefault(tab, null);
    }

    private static Token getOrCreateTabGroupIdWithDefault(
            @NonNull Tab tab, @Nullable Token defaultTabGroupId) {
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) {
            tabGroupId = (defaultTabGroupId == null) ? Token.createRandom() : defaultTabGroupId;
            tab.setTabGroupId(tabGroupId);
        }
        return tabGroupId;
    }

    private static void setBothGroupIds(Tab tab, int rootId, Token tabGroupId) {
        @Nullable TabStateAttributes tabStateAttributes = TabStateAttributes.from(tab);

        // Tab state attributes may be null in unit tests. Permit this only for test builds.
        boolean skipAttributes = BuildConfig.IS_FOR_TEST && tabStateAttributes == null;
        if (!skipAttributes) {
            tabStateAttributes.beginBatchEdit();
        }

        tab.setRootId(rootId);
        tab.setTabGroupId(tabGroupId);

        if (!skipAttributes) {
            tabStateAttributes.endBatchEdit();
        }
    }

    private static boolean shouldSkipGroupCreationDialog(boolean shouldShow) {
        if (ChromeFeatureList.sTabGroupCreationDialogAndroid.isEnabled()) {
            return !shouldShow;
        } else {
            return TabGroupFeatureUtils.SKIP_TAB_GROUP_CREATION_DIALOG.getValue();
        }
    }

    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        RecordHistogram.recordBooleanHistogram(
                "TabGroups.SelectedTabInTabGroup", isTabInTabGroup(tab));
        selectTab(tab);
        if (!shouldNotifyObserversOnSetIndex()) return;
        for (TabModelObserver observer : mFilteredObservers) {
            observer.didSelectTab(tab, type, lastId);
        }
    }

    @Override
    public void willCloseTab(Tab tab, boolean didCloseAlone) {
        closeTab(tab);
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willCloseTab(tab, didCloseAlone);
        }
    }

    @Override
    public void onFinishingTabClosure(Tab tab) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.onFinishingTabClosure(tab);
        }
    }

    @Override
    public void willAddTab(Tab tab, int type) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willAddTab(tab, type);
        }
    }

    @Override
    public void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        addTab(tab, /* fromUndo= */ false);
        for (TabModelObserver observer : mFilteredObservers) {
            observer.didAddTab(tab, type, creationState, markedForSelection);
        }
    }

    @Override
    public void tabPendingClosure(Tab tab) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabPendingClosure(tab);
        }
    }

    @Override
    public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.multipleTabsPendingClosure(tabs, isAllTabs);
        }
    }

    @Override
    public void tabClosureCommitted(Tab tab) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabClosureCommitted(tab);
        }
    }

    @Override
    public void willCloseAllTabs(boolean incognito) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willCloseAllTabs(incognito);
        }
    }

    @Override
    public void allTabsClosureUndone() {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.allTabsClosureUndone();
        }
    }

    @Override
    public void allTabsClosureCommitted(boolean isIncognito) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.allTabsClosureCommitted(isIncognito);
        }
    }

    @Override
    public void tabRemoved(Tab tab) {
        removeTab(tab);
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabRemoved(tab);
        }
    }

    @Override
    public void restoreCompleted() {
        mTabRestoreCompleted = true;

        if (getCount() != 0) reorder();

        for (TabModelObserver observer : mFilteredObservers) {
            observer.restoreCompleted();
        }
    }
}
