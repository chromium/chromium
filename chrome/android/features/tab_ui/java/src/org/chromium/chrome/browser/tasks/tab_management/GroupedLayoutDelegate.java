// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.graphics.Bitmap;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

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
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/**
 * {@link TabListMediator.TabListLayoutType#GROUPED} implementation of {@link
 * TabListLayoutDelegate}.
 */
@NullMarked
class GroupedLayoutDelegate extends TabListLayoutDelegate {
    private final @Nullable ThumbnailProvider mThumbnailProvider;
    private final boolean mUseTabGroupCardType;

    GroupedLayoutDelegate(
            TabListMediator mediator,
            TabListModel modelList,
            @Nullable ThumbnailProvider thumbnailProvider) {
        super(mediator, modelList);
        mThumbnailProvider = thumbnailProvider;
        mUseTabGroupCardType = TabUiFeatureUtilities.isAndroidTabUiRefactorEnabled();
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
        @TabAlert int stateToReturn = super.getAlertState(representativeTab, model);
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
            @TabAlert int currentState = super.getAlertState(tab, model);
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
            Tab representativeTab = tabModel.getRepresentativeTabAt(i);
            if (representativeTab != null && tab.getId() == representativeTab.getId()) {
                tabIndex = i;
                break;
            }
        }

        if (tabIndex == TabList.INVALID_TAB_INDEX) return TabList.INVALID_TAB_INDEX;

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
    // TODO(crbug.com/517544602): Remove this method override when removing the flag.
    @Override
    @Nullable Pair<Integer, Tab> getIndexAndTabForTabGroupId(@Nullable Token tabGroupId) {
        if (mUseTabGroupCardType) return super.getIndexAndTabForTabGroupId(tabGroupId);
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
        // Adds the card for the tab, or no-ops for a child tab of a group.
        super.didAddTab(tab, type);

        if (type == TabLaunchType.FROM_RESTORE) {
            if (mUseTabGroupCardType) {
                updateGroupCard(tab.getTabGroupId(), /* isUpdatingId= */ false);
                return;
            }
            TabModel tabModel = mMediator.getCurrentTabModelChecked();
            int filterIndex = tabModel.representativeIndexOf(tab);
            if (filterIndex == TabList.INVALID_TAB_INDEX) return;
            Tab currentGroupSelectedTab = tabModel.getRepresentativeTabAt(filterIndex);
            assumeNonNull(currentGroupSelectedTab);

            // Refresh the group's card so its thumbnail and title match the group's current
            // state.
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
        // Restores the card for the tab, or no-ops for a child tab of a group.
        super.tabClosureUndone(tab);
        // Updates the group card, if the tab being restored is part of a group.
        if (mUseTabGroupCardType) {
            updateGroupCard(tab.getTabGroupId(), /* isUpdatingId= */ false);
            return;
        }

        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        int filterIndex = tabModel.representativeIndexOf(tab);
        if (filterIndex == TabList.INVALID_TAB_INDEX
                || !tabModel.isTabInTabGroup(tab)
                || filterIndex >= mModelList.size()) {
            return;
        }
        Tab currentGroupSelectedTab = tabModel.getRepresentativeTabAt(filterIndex);
        assumeNonNull(currentGroupSelectedTab);

        // Refresh the group's card so its thumbnail and title match the group's current state.
        int tabListModelIndex = mModelList.indexOfNthTabCard(filterIndex);
        assert getIndexFromTabId(currentGroupSelectedTab.getId()) == tabListModelIndex;

        // TODO(crbug.com/549722494): Clean up updateTab() calls.
        mMediator.updateTab(
                tabListModelIndex,
                currentGroupSelectedTab,
                /* isUpdatingId= */ false,
                /* quickMode= */ false);
    }

    /**
     * Resolves a standalone tab or the group card representing a child tab, scanning related tab
     * IDs while group cards are still keyed by a representative tab ID.
     */
    // TODO(crbug.com/517544602): Delete getUiIndexForTab entirely when removing the flag. Its
    // callers move to getIndexFromTabId.
    @Override
    int getUiIndexForTab(int tabId) {
        if (mUseTabGroupCardType) return getIndexFromTabId(tabId);

        int index = mModelList.indexFromTabId(tabId);
        if (index != TabModel.INVALID_TAB_INDEX) return index;

        // Legacy group cards are keyed by a representative tab ID, so a child tab has to be
        // resolved through the IDs of its related tabs.
        return mMediator.getIndexForTabIdWithRelatedTabs(tabId);
    }

    /** Resolves a standalone tab or the group card representing a child tab. */
    // TODO(crbug.com/517544602): On flag cleanup, callers that already hold a Tab should call
    // getIndexFromTab directly rather than paying for the id round trip here.
    @Override
    int getIndexFromTabId(int tabId) {
        // Standalone tab cards carry their TAB_ID directly and must resolve even after
        // the tab is removed from TabModel during closure.
        int index = mModelList.indexFromTabId(tabId);
        if (index != TabModel.INVALID_TAB_INDEX || !mUseTabGroupCardType) return index;

        // Fall back to resolving the containing group card when the tab has no direct card.
        Tab tab = mMediator.getCurrentTabModelChecked().getTabById(tabId);
        if (tab == null) return TabModel.INVALID_TAB_INDEX;
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return TabModel.INVALID_TAB_INDEX;
        return mModelList.indexFromTabGroupId(tabGroupId);
    }

    // TODO(crbug.com/517544602): Remove this method override when removing the flag.
    @Override
    void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
        // Cards are keyed by a representative tab ID, so on undo, re-key the group's card to
        // the restored tab. This runs before super, which looks the card up by that ID.
        // Token keyed cards need no such update.
        if (!mUseTabGroupCardType && type == TabSelectionType.FROM_UNDO) {
            int newIndex = getUiIndexForTab(tab.getId());
            if (mModelList.isValidIndex(newIndex)) {
                mModelList.updateTabListModelIdForGroup(tab, newIndex);
            }
        }

        // Moves the selection highlight from the old card to the new one.
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
            Pair<Integer, Tab> indexAndTab =
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
            Pair<Integer, Tab> indexAndTab =
                    getIndexAndTabForTabGroupId(updatedTab.getTabGroupId());
            if (indexAndTab == null) return;

            PropertyModel model = mModelList.get(indexAndTab.first).model;
            Tab representativeTab = indexAndTab.second;
            if (!TabUtils.isValid(representativeTab) || model == null) return;

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
            Pair<Integer, Tab> indexAndTab = getIndexAndTabForTabGroupId(tabGroupId);
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
        if (!mMediator.isTrackingTabs()) return;

        if (mMediator.isTabInTabGroup(updatedTab)) {
            int tabId = updatedTab.getId();
            int index = getUiIndexForTab(tabId);
            if (index != TabModel.INVALID_TAB_INDEX) {
                mMediator.updateThumbnailFetcher(mModelList.get(index).model, tabId);
            }
        } else {
            super.onUiTabStateChanged(updatedTab, state);
        }
    }

    @Override
    void onTabClose(Tab tab) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId != null && tabModel.tabGroupExists(tabGroupId)) {
            // If the tab closed was part of a tab group, update the group to reflect the
            // closure instead of closing the tab.
            if (mUseTabGroupCardType) {
                updateGroupCard(tabGroupId, /* isUpdatingId= */ true);
                return;
            }

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

        // Standalone (ungrouped) tabs are removed by the base class.
        super.onTabClose(tab);
    }

    // TabGroupObserver implementation.

    @Override
    public void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {
        Pair<Integer, Tab> indexAndTab = getIndexAndTabForTabGroupId(tabGroupId);
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
        if (tabModelNewIndex == tabModelOldIndex || mThumbnailProvider == null) return;

        int indexInModel = getUiIndexForTab(movedTab.getId());
        if (indexInModel == TabModel.INVALID_TAB_INDEX) return;

        PropertyModel model = mModelList.get(indexInModel).model;
        mMediator.updateThumbnailFetcher(model, movedTab.getId());
    }

    @Override
    public void didMoveTabOutOfGroup(Tab movedTab, Token oldTabGroupId) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Token movedTabGroupId = movedTab.getTabGroupId();
        if (mUseTabGroupCardType) {
            // Add a card for movedTab unless it moved directly into an existing multi-tab group
            // (which is handled by didMergeTabToGroup).
            if (tabModel.getTabCountForGroup(movedTabGroupId) <= 1) {
                mMediator.addTabCardToModel(movedTab, getInsertionIndexOfTab(movedTab));
            }
            // Update the old group's card if it still has tabs left (if the old group dissolved,
            // didRemoveTabGroup removes its card). Skip updating if the group is being removed.
            if (!isRemovingTabGroup(oldTabGroupId)) {
                updateGroupCard(oldTabGroupId, /* isUpdatingId= */ true);
            }
            return;
        }

        Tab lastShownTab = tabModel.getTabById(tabModel.getGroupLastShownTabId(oldTabGroupId));
        Tab previousGroupTab = lastShownTab != null ? lastShownTab : movedTab;

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
                mModelList.indexOfNthTabCard(tabModel.representativeIndexOf(previousGroupTab)),
                previousGroupTab,
                /* isUpdatingId= */ true,
                /* quickMode= */ false);
    }

    @Override
    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
        boolean mergedSourceCard =
                mUseTabGroupCardType
                        ? didMergeTabToGroupByToken(movedTab, isDestinationTab)
                        : didMergeTabToGroupLegacy(movedTab, isDestinationTab);
        if (!mergedSourceCard) return;

        // TODO(crbug.com/434246302): These metrics are probably wrong as it looks
        // like they get emitted per-tab merged, rather than per-group merged.
        if (mMediator.getRelatedTabsForId(movedTab.getId()).size() == 2) {
            // When users use drop-to-merge to create a group.
            RecordUserAction.record("TabGroup.Created.DropToMerge");
        } else {
            RecordUserAction.record("TabGrid.Drag.DropToMerge");
        }
    }

    private boolean didMergeTabToGroupByToken(Tab movedTab, boolean isDestinationTab) {
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        Pair<Integer, Integer> positions =
                getIndexesForMergeToGroupByToken(movedTab, isDestinationTab, relatedTabs);
        int desIndex = positions.first;
        int srcIndex = positions.second;
        if (!mModelList.isValidIndex(desIndex)) return false;

        if (srcIndex == TabModel.INVALID_TAB_INDEX) {
            // Update the destination group card.
            mMediator.updateTab(desIndex, movedTab, true, false);
            if (isDestinationTab) {
                // Only reposition when movedTab is the target tab starting the group; when
                // another tab joins an existing group, the group card stays in its spot.
                mModelList.moveItem(desIndex, getInsertionIndexOfTab(movedTab));
            }
            return false;
        }

        if (!mModelList.isValidIndex(srcIndex)) return false;

        // Remove the merged source card and update the destination group card.
        mModelList.removeAt(srcIndex);
        desIndex = srcIndex > desIndex ? desIndex : mModelList.getTabIndexBefore(desIndex);
        mMediator.updateTab(desIndex, movedTab, true, false);
        return true;
    }

    // TODO(crbug.com/517544602): Delete when removing the flag.
    private boolean didMergeTabToGroupLegacy(Tab movedTab, boolean isDestinationTab) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        Pair<Integer, Integer> positions =
                getIndexesForMergeToGroupLegacy(tabModel, movedTab, isDestinationTab, relatedTabs);
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
            return false;
        }

        if (!mModelList.isValidIndex(srcIndex) || !mModelList.isValidIndex(desIndex)) {
            return false;
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
        return true;
    }

    @Override
    public void didMoveTabGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        int curPosition;
        if (mUseTabGroupCardType) {
            curPosition = getIndexFromTab(movedTab);
        } else {
            Tab currentGroupSelectedTab =
                    TabGroupUtils.getSelectedTabInGroupForTab(tabModel, movedTab);
            curPosition = getIndexFromTabId(currentGroupSelectedTab.getId());
            if (curPosition == TabModel.INVALID_TAB_INDEX) {
                // Sync TabListModel with updated TabModel.
                int indexToUpdate =
                        mModelList.indexOfNthTabCard(
                                tabModel.representativeIndexOf(
                                        tabModel.getTabAt(tabModelOldIndex)));
                mModelList.updateTabListModelIdForGroup(currentGroupSelectedTab, indexToUpdate);
                curPosition = getIndexFromTabId(currentGroupSelectedTab.getId());
            }
        }
        if (!mModelList.isValidIndex(curPosition)) return;

        // TODO(crbug.com/517544602): We can use getInsertionIndexOfTab
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
        int newPosition;
        if (mUseTabGroupCardType) {
            newPosition = getIndexFromTab(destinationTab);
            assert newPosition != TabList.INVALID_TAB_INDEX;
        } else {
            Tab destinationGroupSelectedTab =
                    TabGroupUtils.getSelectedTabInGroupForTab(tabModel, destinationTab);
            newPosition = getIndexFromTabId(destinationGroupSelectedTab.getId());
            if (newPosition == TabModel.INVALID_TAB_INDEX) {
                int indexToUpdate =
                        mModelList.indexOfNthTabCard(
                                tabModel.representativeIndexOf(destinationTab)
                                        + (tabModelNewIndex > tabModelOldIndex ? 1 : -1));
                mModelList.updateTabListModelIdForGroup(destinationGroupSelectedTab, indexToUpdate);
                newPosition = getIndexFromTabId(destinationGroupSelectedTab.getId());
            }
        }
        mModelList.moveItem(curPosition, newPosition);
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
    @ModelType
    int getGroupCardType() {
        return mUseTabGroupCardType ? ModelType.TAB_GROUP : ModelType.TAB;
    }

    @Override
    boolean areTabsInSameGroup(PropertyModel model, Tab newTab) {
        Token newGroupId = newTab.getTabGroupId();
        if (newGroupId == null) return false;
        if (mUseTabGroupCardType) {
            return model.get(CARD_TYPE) == ModelType.TAB_GROUP
                    && newGroupId.equals(model.get(TabProperties.TAB_GROUP_HEADER_ID));
        }
        int previousTabId = TabProperties.getTabId(model);
        Tab previousTab = mMediator.getCurrentTabModelChecked().getTabById(previousTabId);
        return previousTab != null && newGroupId.equals(previousTab.getTabGroupId());
    }

    /**
     * This method gets indexes in the {@link TabListModel} of the tab cards that are merged into a
     * group. This should always produce a valid destination index which is the index in the {@link
     * TabListModel} that the moved tab should exist in. The source index may be invalid if a group
     * of size 1 is created or the tab was moved between groups. In the case of moving between
     * groups as the other group will be updated by {@link
     * TabGroupObserver#didMoveTabOutOfGroup(Tab, Token)}.
     *
     * @param tabModel The tabModel that owns the tabs.
     * @param movedTab The tab that is being merged.
     * @param isDestinationTab Whether the moved tab is being merged to the group or is the
     *     destination.
     * @param tabs The list that contains tabs of the newly merged group.
     * @return A Pair with its first member as the index that is merged to and the second member as
     *     the index that is being merged from.
     */
    @VisibleForTesting
    Pair<Integer, Integer> getIndexesForMergeToGroup(
            TabModel tabModel, Tab movedTab, boolean isDestinationTab, List<Tab> tabs) {
        return mUseTabGroupCardType
                ? getIndexesForMergeToGroupByToken(movedTab, isDestinationTab, tabs)
                : getIndexesForMergeToGroupLegacy(tabModel, movedTab, isDestinationTab, tabs);
    }

    /**
     * Returns the destination and source card indexes for a merge when group cards are keyed by
     * token. At most two cards are involved: the one showing {@code movedTab} and the one showing
     * the tabs it merged with. A card left behind by a fully dissolved group is removed by {@link
     * #didRemoveTabGroup}, which runs after this callback.
     *
     * @param movedTab The tab that is being merged.
     * @param isDestinationTab Whether the moved tab is being merged to the group or is the
     *     destination.
     * @param tabs The list that contains tabs of the newly merged group.
     * @return A Pair with its first member as the index that is merged to and the second member as
     *     the index that is being merged from.
     */
    private Pair<Integer, Integer> getIndexesForMergeToGroupByToken(
            Tab movedTab, boolean isDestinationTab, List<Tab> tabs) {
        int movedIndex = getIndexFromTab(movedTab);

        int otherIndex = TabModel.INVALID_TAB_INDEX;
        for (Tab tab : tabs) {
            if (tab == movedTab) continue;

            int index = getIndexFromTab(tab);
            // All tabs of a group share one card, ignore a repeat hit on movedIndex.
            if (index != TabModel.INVALID_TAB_INDEX && index != movedIndex) {
                otherIndex = index;
                break;
            }
        }

        // A missing other index is allowed; tab group undo can transiently hide it from the model.
        if (isDestinationTab || otherIndex == TabModel.INVALID_TAB_INDEX) {
            return Pair.create(movedIndex, otherIndex);
        }
        return Pair.create(otherIndex, movedIndex);
    }

    // TODO(crbug.com/517544602): Remove this method when deleting the feature flag.
    private Pair<Integer, Integer> getIndexesForMergeToGroupLegacy(
            TabModel tabModel, Tab movedTab, boolean isDestinationTab, List<Tab> tabs) {
        // The moved tab is always involved in the merge, but it may not have an index if it was
        // moved between groups.
        int movedTabListModelIndex = mModelList.indexFromTabId(movedTab.getId());

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

            otherTabListModelIndex = mModelList.indexFromTabId(curTab.getId());
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

    /**
     * Updates the card of {@code tabGroupId} so it matches the group's current state. No-ops if
     * {@code tabGroupId} is null, not in the model list, or has no tabs.
     *
     * @param tabGroupId The {@link Token} identifying the tab group.
     * @param isUpdatingId Whether the card should be re-keyed to the resolved member tab.
     */
    private void updateGroupCard(@Nullable Token tabGroupId, boolean isUpdatingId) {
        Pair<Integer, Tab> indexAndTab = getIndexAndTabForTabGroupId(tabGroupId);
        if (indexAndTab == null) return;

        mMediator.updateTab(
                indexAndTab.first, indexAndTab.second, isUpdatingId, /* quickMode= */ false);
    }

    /**
     * Resolves the card currently showing {@code tab}: its own card, or its tab group card.
     *
     * @param tab The tab to resolve the card index for.
     * @return The index in the model list, or {@link TabModel#INVALID_TAB_INDEX}.
     */
    private int getIndexFromTab(Tab tab) {
        int index = mModelList.indexFromTabId(tab.getId());
        if (index != TabModel.INVALID_TAB_INDEX) return index;

        // A group Token is immutable for the life of the group, unlike the representative tab ID.
        // This only matches a card whose CARD_TYPE is TAB_GROUP.
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return TabModel.INVALID_TAB_INDEX;
        return mModelList.indexFromTabGroupId(tabGroupId);
    }
}
