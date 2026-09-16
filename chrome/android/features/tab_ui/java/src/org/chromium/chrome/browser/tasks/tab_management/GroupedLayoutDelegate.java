// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.util.Pair;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;

/**
 * {@link TabListMediator.TabListLayoutType#GROUPED} implementation of {@link
 * TabListLayoutDelegate}.
 */
@NullMarked
class GroupedLayoutDelegate extends TabListLayoutDelegate {
    private final @Nullable ThumbnailProvider mThumbnailProvider;

    GroupedLayoutDelegate(
            TabListMediator mediator,
            TabListModel modelList,
            @Nullable ThumbnailProvider thumbnailProvider) {
        super(mediator, modelList);
        mThumbnailProvider = thumbnailProvider;
    }

    @Override
    boolean requiresThumbnailUpdateOnDeselect() {
        return true;
    }

    @Override
    boolean requiresThumbnailUpdateOnSelect() {
        return true;
    }

    @Override
    boolean supportsTabGroups() {
        return true;
    }

    @Override
    boolean isChildTabRepresentedByGroupCard(Tab tab) {
        return mMediator.getCurrentTabModelChecked().isTabInTabGroup(tab);
    }

    @Override
    @TabAlert
    int getAlertState(Tab representativeTab, PropertyModel model) {
        @TabAlert int stateToReturn = representativeTab.getAlertState();
        int statePriority = TabUtils.getTabAlertPriority(stateToReturn);
        // Fast exit if not in a group or already at maximum priority state.
        if (!mMediator.isTabInTabGroup(representativeTab)
                || statePriority == TabUtils.MAX_TAB_ALERT_PRIORITY) {
            return stateToReturn;
        }

        // Check all tabs in the group to surface the highest priority alert state onto the group
        // card.
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(representativeTab.getId());
        for (Tab tab : relatedTabs) {
            @TabAlert int currentState = tab.getAlertState();
            int currentPriority = TabUtils.getTabAlertPriority(currentState);
            if (currentPriority > statePriority) {
                statePriority = currentPriority;
                stateToReturn = currentState;
            }
            if (statePriority == TabUtils.MAX_TAB_ALERT_PRIORITY) return stateToReturn;
        }
        return stateToReturn;
    }

    @Override
    int getInsertionIndexOfTab(Tab tab) {
        if (tab == null) return TabList.INVALID_TAB_INDEX;
        int tabIndex = TabList.INVALID_TAB_INDEX;
        TabModel tabModel = mMediator.getCurrentTabModelChecked();

        // Compute the index of the tab out of all tabs in the filter (ignore tabs that are not
        // the representative tab in a group).
        int count = tabModel.getIndividualTabAndGroupCount();
        for (int i = 0; i < count; i++) {
            @Nullable Tab representativeTab = tabModel.getRepresentativeTabAt(i);
            if (representativeTab != null && tab.getId() == representativeTab.getId()) {
                tabIndex = i;
                break;
            }
        }

        // The current implementation of ARCHIVED_TAB_GROUP card types places all groups at the
        // beginning of the model list. As a result, if any tab group cards exist, adjust the index
        // for tab insertion to start after the allotted count of tab groups in the model list.
        tabIndex += mModelList.getArchivedTabGroupCardCount();

        // Get the position of the nth tab card ignoring any other CARD_TYPE entries present in the
        // model list outside of TAB, TAB_GROUP, and ARCHIVED_TAB_GROUP.
        return mModelList.indexOfNthTabCard(tabIndex);
    }

    /**
     * Returns the index in {@link #mModelList} of the group with {@code tabGroupId} and the {@link
     * Tab} representing the group. Will be null if the entry is not present, the tab cannot be
     * found, or the tab is not part of a tab group.
     */
    @Override
    @Nullable Pair<Integer, Tab> getIndexAndTabForTabGroupId(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return null;

        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        @TabId int lastShownTabId = tabModel.getGroupLastShownTabId(tabGroupId);

        int index = getUiIndexForTab(lastShownTabId);
        if (index == TabModel.INVALID_TAB_INDEX) return null;

        Tab tab = mMediator.getTabForIndex(index);
        // If the found tab has a different group ID from the tabGroupId set in the args then the
        // update is likely for a group that no longer exists so we should drop the update.
        if (tab == null
                || !tabGroupId.equals(tab.getTabGroupId())
                || !tabModel.isTabInTabGroup(tab)) {
            return null;
        }
        return Pair.create(index, tab);
    }

    @Override
    void didAddTab(Tab tab, @TabLaunchType int type) {
        super.didAddTab(tab, type);

        if (type == TabLaunchType.FROM_RESTORE) {
            TabModel tabModel = mMediator.getCurrentTabModelChecked();
            int filterIndex = tabModel.representativeIndexOf(tab);
            if (filterIndex == TabList.INVALID_TAB_INDEX) return;
            Tab currentGroupSelectedTab = tabModel.getRepresentativeTabAt(filterIndex);
            assumeNonNull(currentGroupSelectedTab);

            int tabListModelIndex = mModelList.indexOfNthTabCard(filterIndex);
            if (getIndexFromTabId(currentGroupSelectedTab.getId()) != tabListModelIndex) {
                return;
            }
            mMediator.updateTab(
                    tabListModelIndex,
                    currentGroupSelectedTab,
                    /* isUpdatingId= */ false,
                    /* quickMode= */ false);
        }
    }

    @Override
    void tabClosureUndone(Tab tab) {
        super.tabClosureUndone(tab);

        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        int filterIndex = tabModel.representativeIndexOf(tab);
        if (filterIndex == TabList.INVALID_TAB_INDEX
                || !tabModel.isTabInTabGroup(tab)
                || filterIndex >= mModelList.size()) {
            return;
        }
        Tab currentGroupSelectedTab = tabModel.getRepresentativeTabAt(filterIndex);
        assumeNonNull(currentGroupSelectedTab);

        int tabListModelIndex = mModelList.indexOfNthTabCard(filterIndex);
        assert getIndexFromTabId(currentGroupSelectedTab.getId()) == tabListModelIndex;

        // TODO(crbug.com/549722494): Clean up updateTab() calls.
        mMediator.updateTab(tabListModelIndex, currentGroupSelectedTab, false, false);
    }

    /**
     * Resolves the UI index in {@link #mModelList} for the given tab ID, falling back to finding
     * the containing tab group card if the tab is a non-representative member of a group.
     */
    @Override
    int getUiIndexForTab(int tabId) {
        int index = getIndexFromTabId(tabId);
        if (index == TabModel.INVALID_TAB_INDEX) {
            // If a tab in a tab group does not have its own card in the model, identify the
            // related tab IDs and determine the index of the group card in the model list.
            index = mMediator.getIndexForTabIdWithRelatedTabs(tabId);
        }
        return index;
    }

    /** Resolves a tab to its own card, or to its tab group card if the tab is in a group. */
    @Override
    int getIndexFromTabId(int tabId) {
        Tab tab = mMediator.getCurrentTabModelChecked().getTabById(tabId);
        if (tab != null) {
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null) {
                // A group Token is immutable for the life of the group, unlike the representative
                // tab ID. This only matches a card whose CARD_TYPE is TAB_GROUP.
                int headerIndex = mModelList.indexFromTabGroupId(tabGroupId);
                if (headerIndex != TabModel.INVALID_TAB_INDEX) return headerIndex;
            }
        }
        // Either the tab is not in a group, or the group has no TAB_GROUP card.
        return mModelList.indexFromTabId(tabId);
    }

    @Override
    void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
        // For UNDO ensure we update the representative tab in the model.
        if (type == TabSelectionType.FROM_UNDO) {
            int newIndex = getUiIndexForTab(tab.getId());
            if (mModelList.isValidIndex(newIndex)) {
                mModelList.updateTabListModelIdForGroup(tab, newIndex);
            }
        }

        super.didSelectTab(tab, type, lastId);
    }

    @Override
    void recordTabSelection(int tabId) {
        // Tab switching metrics for GROUPED layout (GTS) are filtered out here and tracked at the
        // pane/switcher level (see HubTabSwitcherMetricsRecorder#onTabSelected).
        //
        // In GTS, components can switch to a different TabModel before switching tabs, whereas
        // TabListMediator only contains tabs that are in the same TabModel. Additionally, for
        // MobileTabSwitched, GTS must account for MobileTabReturnedToCurrentTab (returning to the
        // same tab as before entering the switcher), which is not tracked at this level.
    }

    // TabObserver implementation.

    @Override
    public void onFaviconUpdated(Tab updatedTab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
        if (!mMediator.isTrackingTabs()) return;

        if (mMediator.isTabInTabGroup(updatedTab)) {
            @Nullable Pair<Integer, Tab> indexAndTab =
                    getIndexAndTabForTabGroupId(updatedTab.getTabGroupId());
            if (indexAndTab == null) return;

            PropertyModel model = mModelList.get(indexAndTab.first).model;
            Tab representativeTab = indexAndTab.second;

            mMediator.updateThumbnailFetcher(model, representativeTab.getId());
            mMediator.updateFaviconForTab(model, representativeTab, icon, iconUrl);
        } else {
            super.onFaviconUpdated(updatedTab, icon, iconUrl);
        }
    }

    @Override
    public void onUrlUpdated(Tab updatedTab) {
        if (!mMediator.isTrackingTabs()) return;

        if (mMediator.isTabInTabGroup(updatedTab)) {
            @Nullable Pair<Integer, Tab> indexAndTab =
                    getIndexAndTabForTabGroupId(updatedTab.getTabGroupId());
            if (indexAndTab == null) return;

            PropertyModel model = mModelList.get(indexAndTab.first).model;
            Tab representativeTab = indexAndTab.second;
            if (!TabUtils.isValid(representativeTab) || model == null) return;

            model.set(
                    TabProperties.URL_DOMAIN, mMediator.getDomainForTab(representativeTab, model));
            mMediator.updateThumbnailFetcher(model, representativeTab.getId());
            mMediator.updateFaviconForTab(model, representativeTab, null, null);
        } else {
            super.onUrlUpdated(updatedTab);
        }
    }

    @Override
    public void onAlertStateChanged(Tab updatedTab, @TabAlert int alertState) {
        if (!mMediator.isTrackingTabs()) return;

        if (mMediator.isTabInTabGroup(updatedTab)) {
            Token tabGroupId = updatedTab.getTabGroupId();
            assumeNonNull(tabGroupId);
            @Nullable Pair<Integer, Tab> indexAndTab = getIndexAndTabForTabGroupId(tabGroupId);
            if (indexAndTab == null) return;

            PropertyModel model = mModelList.get(indexAndTab.first).model;
            if (model == null || model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION)) {
                return;
            }
            Tab representativeTab = indexAndTab.second;
            @TabAlert int alertStateToSet = getAlertState(representativeTab, model);
            model.set(TabProperties.ALERT_STATE, alertStateToSet);
            mMediator.updateDescriptionString(model);
        } else {
            super.onAlertStateChanged(updatedTab, alertState);
        }
    }

    /**
     * When a tab in a tab group changes Actor UI state, refresh the group card thumbnail to reflect
     * the update.
     */
    @Override
    void onUiTabStateChanged(Tab updatedTab, UiTabState state) {
        if (mMediator.isTabInTabGroup(updatedTab)) {
            // Resolves the containing group card and refreshes its thumbnail.
            int index = getUiIndexForTab(updatedTab.getId());
            if (index != TabModel.INVALID_TAB_INDEX) {
                PropertyModel groupModel = mModelList.get(index).model;
                mMediator.updateThumbnailFetcher(groupModel, groupModel.get(TabProperties.TAB_ID));
            }
        }
    }

    @Override
    void onTabClose(Tab tab) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId != null && tabModel.tabGroupExists(tabGroupId)) {
            // If the tab closed was part of a tab group and the closure was
            // triggered from a grouped layout, update the group to reflect the
            // closure instead of closing the tab.
            int groupIndex = tabModel.representativeIndexOf(tab);
            Tab groupTab = tabModel.getRepresentativeTabAt(groupIndex);
            assumeNonNull(groupTab);
            if (!groupTab.isClosing()) {
                mMediator.updateTab(
                        mModelList.indexOfNthTabCard(groupIndex),
                        groupTab,
                        /* isUpdatingId= */ true,
                        /* quickMode= */ false);
                return;
            }
        }

        super.onTabClose(tab);
    }

    // TabGroupObserver implementation.

    @Override
    public void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {
        @Nullable Pair<Integer, Tab> indexAndTab = getIndexAndTabForTabGroupId(tabGroupId);
        if (indexAndTab == null) return;
        Tab tab = indexAndTab.second;
        PropertyModel model = mModelList.get(indexAndTab.first).model;

        mMediator.updateTabGroupProperties(tab, model, newColor);
        mMediator.updateFaviconForTab(model, tab, null, null);
        mMediator.updateDescriptionString(model);
        mMediator.updateActionButtonDescriptionString(tab, model);
        mMediator.updateThumbnailFetcher(model, tab.getId());
    }

    /**
     * When a tab moves within its tab group, only the group card thumbnail needs to be updated to
     * reflect the new tab ordering.
     */
    @Override
    public void didMoveWithinGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        if (tabModelNewIndex == tabModelOldIndex) return;
        if (mThumbnailProvider == null) {
            return;
        }
        int indexInModel = getUiIndexForTab(movedTab.getId());
        if (indexInModel == TabModel.INVALID_TAB_INDEX) return;

        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab lastShownTab =
                tabModel.getRepresentativeTabAt(tabModel.representativeIndexOf(movedTab));
        assumeNonNull(lastShownTab);
        PropertyModel model = mModelList.get(indexInModel).model;
        mMediator.updateThumbnailFetcher(model, lastShownTab.getId());
    }

    @Override
    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab previousGroupTab = tabModel.getRepresentativeTabAt(prevFilterIndex);
        assumeNonNull(previousGroupTab);

        Token movedTabGroupId = movedTab.getTabGroupId();
        if (tabModel.getTabCountForGroup(movedTabGroupId) <= 1 && movedTab != previousGroupTab) {
            // Add a tab to the model if it represents a new card. This happens if
            // the tab is either not in a group or in a group by itself. We do this
            // first so that the indices for the filter and the model match when
            // doing the update afterwards. When moving a tab between groups, the
            // new tab being added to an existing group is handled in
            // didMergeTabToGroup().
            int filterIndex = tabModel.representativeIndexOf(movedTab);
            mMediator.addTabCardToModel(movedTab, mModelList.indexOfNthTabCard(filterIndex));
        } else if (movedTabGroupId != null
                && movedTabGroupId.equals(previousGroupTab.getTabGroupId())) {
            // Despite being ungrouped we are still in a tab group this could mean
            // the previous tab card this tab was associated with no longer contains
            // tabs. If we have the same tab group id as the previous group tab then
            // this was possibly the last tab in its group. Remove the tab card if
            // it exists.
            int previousIndex = getIndexFromTabId(movedTab.getId());
            if (previousIndex != TabModel.INVALID_TAB_INDEX) {
                mModelList.removeAt(previousIndex);
                return;
            }
        }
        // Always update the previous group to clean up old state e.g. thumbnail,
        // title, etc.
        mMediator.updateTab(
                mModelList.indexOfNthTabCard(prevFilterIndex), previousGroupTab, true, false);
    }

    @Override
    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        Pair<Integer, Integer> positions =
                getIndexesForMergeToGroup(tabModel, movedTab, isDestinationTab, relatedTabs);
        int srcIndex = positions.second;
        int desIndex = positions.first;

        // If only the desIndex is valid then just update the destination index to
        // the last shown tab in its group.
        if (desIndex != TabModel.INVALID_TAB_INDEX && srcIndex == TabModel.INVALID_TAB_INDEX) {
            @TabId int desIndexTabId = mModelList.get(desIndex).model.get(TabProperties.TAB_ID);
            Tab desTab = tabModel.getTabById(desIndexTabId);
            assumeNonNull(desTab);
            Token desTabGroupId = desTab.getTabGroupId();
            Tab lastShownTab = desTab;
            if (desTabGroupId != null) {
                @TabId int lastShownTabId = tabModel.getGroupLastShownTabId(desTabGroupId);
                if (lastShownTabId != Tab.INVALID_TAB_ID) {
                    lastShownTab = tabModel.getTabById(lastShownTabId);
                }
            }
            assert lastShownTab != null;
            mMediator.updateTab(desIndex, lastShownTab, true, false);
            int targetIndex = getInsertionIndexOfTab(lastShownTab);
            mModelList.moveItem(desIndex, targetIndex);
            return;
        }

        if (!mModelList.isValidIndex(srcIndex) || !mModelList.isValidIndex(desIndex)) {
            return;
        }

        // We merged the source group to the destination group. Remove the source
        // group and update the destination group.
        mModelList.removeAt(srcIndex);
        desIndex = srcIndex > desIndex ? desIndex : mModelList.getTabIndexBefore(desIndex);
        Tab newSelectedTabInMergedGroup =
                tabModel.getRepresentativeTabAt(mModelList.getTabCardCountsBefore(desIndex));
        assumeNonNull(newSelectedTabInMergedGroup);
        if (newSelectedTabInMergedGroup != null) {
            mMediator.updateTab(desIndex, newSelectedTabInMergedGroup, true, false);
        }

        // TODO(crbug.com/434246302): These metrics are probably wrong as it looks
        // like they get emitted per-tab merged, rather than per-group merged.
        if (mMediator.getRelatedTabsForId(movedTab.getId()).size() == 2) {
            // When users use drop-to-merge to create a group.
            RecordUserAction.record("TabGroup.Created.DropToMerge");
        } else {
            RecordUserAction.record("TabGrid.Drag.DropToMerge");
        }
    }

    @Override
    public void didMoveTabGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab currentGroupSelectedTab = TabGroupUtils.getSelectedTabInGroupForTab(tabModel, movedTab);
        int curPosition = getIndexFromTabId(currentGroupSelectedTab.getId());
        if (curPosition == TabModel.INVALID_TAB_INDEX) {
            // Sync TabListModel with updated TabModel.
            int indexToUpdate =
                    mModelList.indexOfNthTabCard(
                            tabModel.representativeIndexOf(tabModel.getTabAt(tabModelOldIndex)));
            mModelList.updateTabListModelIdForGroup(currentGroupSelectedTab, indexToUpdate);
            curPosition = getIndexFromTabId(currentGroupSelectedTab.getId());
        }
        if (!mModelList.isValidIndex(curPosition)) return;

        // TODO(crbug.com/526117174): We can use getInsertionIndexOfTab(currentGroupSelectedTab)
        // to determine the new position, instead of manual offset math and looking up
        // adjacent tabs.

        // Find the tab which was in the destination index before this move. Use
        // that tab to figure out the new position.
        int destinationTabIndex =
                tabModelNewIndex > tabModelOldIndex
                        ? tabModelNewIndex - relatedTabs.size()
                        : tabModelNewIndex + 1;
        Tab destinationTab = tabModel.getTabAt(destinationTabIndex);
        assumeNonNull(destinationTab);
        Tab destinationGroupSelectedTab =
                TabGroupUtils.getSelectedTabInGroupForTab(tabModel, destinationTab);
        int newPosition = getIndexFromTabId(destinationGroupSelectedTab.getId());
        if (newPosition == TabModel.INVALID_TAB_INDEX) {
            int indexToUpdate =
                    mModelList.indexOfNthTabCard(
                            tabModel.representativeIndexOf(destinationTab)
                                    + (tabModelNewIndex > tabModelOldIndex ? 1 : -1));
            mModelList.updateTabListModelIdForGroup(destinationGroupSelectedTab, indexToUpdate);
            newPosition = getIndexFromTabId(destinationGroupSelectedTab.getId());
        }
        mModelList.moveItem(curPosition, newPosition);
    }

    @Override
    public void didCreateNewGroup(Tab destinationTab, TabModel tabModel) {
        // On new group creation for the tab group representation in the GTS, update
        // the tab group color icon.
        int groupIndex = tabModel.representativeIndexOf(destinationTab);
        Tab groupTab = tabModel.getRepresentativeTabAt(groupIndex);
        assumeNonNull(groupTab);
        PropertyModel model = getModelFromTabId(groupTab.getId());

        if (model != null) {
            Token tabGroupId = destinationTab.getTabGroupId();
            assumeNonNull(tabGroupId);
            @TabGroupColorId int colorId = tabModel.getTabGroupColorWithFallback(tabGroupId);
            mMediator.updateTabGroupProperties(destinationTab, model, colorId);
            mMediator.updateFaviconForTab(model, groupTab, null, null);
        }
    }

    @Override
    void onTabSelectionToggled(PropertyModel model, int tabId, boolean wasSelected) {
        // Reset thumbnail to ensure the color of the blank tab slots is correct.
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab tab = tabModel.getTabById(tabId);
        if (tab != null && tabModel.isTabInTabGroup(tab)) {
            mMediator.updateThumbnailFetcher(model, tabId);
        }
    }

    @Override
    boolean areTabsInSameGroup(int previousTabId, Tab newTab) {
        Tab previousTab = mMediator.getCurrentTabModelChecked().getTabById(previousTabId);
        return previousTab != null
                && previousTab.getTabGroupId() != null
                && Objects.equals(previousTab.getTabGroupId(), newTab.getTabGroupId());
    }

    /**
     * This method gets indexes in the {@link TabListModel} of the tab cards that are merged into a
     * group. This should always produce a valid destination index which is the index in the {@link
     * TabListModel} that the moved tab should exist in. The source index may be invalid if a group
     * of size 1 is created or the tab was moved between groups. In the case of moving between
     * groups as the other group will be updated by {@link
     * TabGroupObserver#didMoveTabOutOfGroup(Tab, int)}.
     *
     * @param tabModel The tabModel that owns the tabs.
     * @param movedTab The tab that is being merged.
     * @param isDestinationTab Whether the moved tab is being merged to the group or is the
     *     destination.
     * @param tabs The list that contains tabs of the newly merged group.
     * @return A Pair with its first member as the index that is merged to and the second member as
     *     the index that is being merged from.
     */
    Pair<Integer, Integer> getIndexesForMergeToGroup(
            TabModel tabModel, Tab movedTab, boolean isDestinationTab, List<Tab> tabs) {
        // The moved tab is always involved in the merge, but it may not have an index if it was
        // moved between groups.
        int movedTabListModelIndex = getIndexFromTabId(movedTab.getId());

        // TODO(crbug.com/433947821): The use of TabModel here is probably overkill. Consider
        // iterating through just tabs.

        // Find the other index that is involved in the merge it should be in the list of tabs.
        int otherTabListModelIndex = TabModel.INVALID_TAB_INDEX;
        int startIndex = tabModel.indexOf(tabs.get(0));
        int endIndex = tabModel.indexOf(tabs.get(tabs.size() - 1));
        // Ensure the last tab is last in the model and the first tab is the first.
        assert endIndex - startIndex == tabs.size() - 1;
        for (int i = startIndex; i <= endIndex; i++) {
            Tab curTab = tabModel.getTabAtChecked(i);
            // Group should be contiguous.
            assert tabs.contains(curTab);
            if (curTab == movedTab) continue;

            otherTabListModelIndex = getIndexFromTabId(curTab.getId());
            if (otherTabListModelIndex != TabModel.INVALID_TAB_INDEX) break;
        }

        // If nothing is found in the model early return, this might be a case of tab group undo.
        if (movedTabListModelIndex == TabModel.INVALID_TAB_INDEX
                && otherTabListModelIndex == TabModel.INVALID_TAB_INDEX) {
            return new Pair<>(TabModel.INVALID_TAB_INDEX, TabModel.INVALID_TAB_INDEX);
        }

        final int desIndex;
        final int srcIndex;
        if (isDestinationTab || otherTabListModelIndex == TabModel.INVALID_TAB_INDEX) {
            // We allow failing to find the other index as it might be a case of tab group undo
            // which has a intermediate sequencing and model updates that can result in failing to
            // find the tab among the related tabs.

            // The moved tab is the destination tab and should always be in the model.
            assert movedTabListModelIndex != TabModel.INVALID_TAB_INDEX;

            desIndex = movedTabListModelIndex;
            srcIndex = otherTabListModelIndex;
        } else {
            // The other tab is the destination tab and should always be in the model.
            desIndex = otherTabListModelIndex;
            srcIndex = movedTabListModelIndex;
        }
        return new Pair<>(desIndex, srcIndex);
    }
}
