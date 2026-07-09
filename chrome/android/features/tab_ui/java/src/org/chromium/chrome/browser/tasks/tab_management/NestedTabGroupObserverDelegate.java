// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * {@link TabListMediator.TabListLayoutType#NESTED} implementation of {@link
 * TabGroupObserverDelegate}.
 */
@NullMarked
class NestedTabGroupObserverDelegate extends TabGroupObserverDelegate {
    NestedTabGroupObserverDelegate(TabListMediator mediator, TabListModel modelList) {
        super(mediator, modelList);
    }

    @Override
    public void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {
        super.didChangeTabGroupColor(tabGroupId, newColor);
        // Sync the color down to the child models so decorations (like the group spine) can
        // read it.
        updateColorForChildTabsInNestedLayout(tabGroupId, newColor);
    }

    @Override
    public void didChangeTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate) {
        int headerIndex = mModelList.indexFromTabGroupId(tabGroupId);
        if (headerIndex == TabModel.INVALID_TAB_INDEX) return;
        PropertyModel model = mModelList.get(headerIndex).model;

        if (isCollapsed == TabProperties.isTabGroupCollapsed(model)) {
            return;
        }

        model.set(TabProperties.IS_COLLAPSED, isCollapsed);

        if (isCollapsed) {
            removeChildTabs(tabGroupId);
        } else {
            mMediator.insertChildTabs(tabGroupId, headerIndex);
        }
    }

    @Override
    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab previousGroupTab = tabModel.getRepresentativeTabAt(prevFilterIndex);
        assumeNonNull(previousGroupTab);

        Token oldTabGroupId = previousGroupTab.getTabGroupId();
        mMediator.updateTabGroupHeaderId(oldTabGroupId);
        syncChildTab(movedTab, oldTabGroupId);
    }

    @Override
    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
        syncChildTab(movedTab, /* oldTabGroupId= */ null);
    }

    @Override
    public void didMoveTabGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        // Move the grouo header along with all the child tabs.
        Token tabGroupId = movedTab.getTabGroupId();
        assert tabGroupId != null;

        int sourceUiIndex = mModelList.indexFromTabGroupId(tabGroupId);
        if (sourceUiIndex == TabModel.INVALID_TAB_INDEX) return;

        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        if (relatedTabs == null || relatedTabs.isEmpty()) return;

        int itemsToMove = 1;
        PropertyModel headerModel = mModelList.get(sourceUiIndex).model;
        boolean isCollapsed = TabProperties.isTabGroupCollapsed(headerModel);
        if (!isCollapsed) {
            itemsToMove += relatedTabs.size();
        }

        int destinationUiIndex =
                getInsertionIndexOfGroupForNestedLayout(movedTab, tabModelNewIndex, relatedTabs);
        if (destinationUiIndex == TabModel.INVALID_TAB_INDEX) return;

        if (sourceUiIndex + itemsToMove == destinationUiIndex) return;

        if (sourceUiIndex < destinationUiIndex) {
            // Move the tab group down. Insert it immediately before the destination's UI position.
            for (int i = 0; i < itemsToMove; i++) {
                mModelList.move(sourceUiIndex, destinationUiIndex - 1);
            }
        } else if (sourceUiIndex > destinationUiIndex) {
            // Move the tab group up. Insert it exactly at the destination's UI position.
            for (int i = 0; i < itemsToMove; i++) {
                mModelList.move(sourceUiIndex + i, destinationUiIndex + i);
            }
        }
    }

    @Override
    public void didCreateNewGroup(Tab destinationTab, TabModel tabModel) {
        Token tabGroupId = destinationTab.getTabGroupId();
        if (tabGroupId == null) return;

        int destUiIndex = mModelList.indexFromTabId(destinationTab.getId());
        if (destUiIndex == TabModel.INVALID_TAB_INDEX) return;

        if (mMediator.ensureGroupHeaderExistsInNestedLayout(
                destinationTab, tabGroupId, destUiIndex)) {
            // After adding the group header, the destination tab's model shifts by one position.
            PropertyModel childModel = mModelList.get(destUiIndex + 1).model;
            mMediator.setupGroupPropertiesForChildTab(destinationTab, childModel);
        }
    }

    @Override
    public void didRemoveTabGroup(
            int oldRootId,
            @Nullable Token oldTabGroupId,
            @DidRemoveTabGroupReason int removalReason) {
        if (oldTabGroupId == null) {
            return;
        }
        // When a group is destroyed (due to tab closures, ungrouping, etc.), the corresponding
        // Group Header card needs to be removed as well.
        int index = mModelList.indexFromTabGroupId(oldTabGroupId);
        if (index != TabModel.INVALID_TAB_INDEX) {
            mModelList.removeAt(index);
        }
    }

    /**
     * Updates the UI properties and positioning of a child tab in the NESTED layout when its group
     * membership changes.
     *
     * @param tab The tab whose group state is being updated.
     * @param oldTabGroupId The previous group ID of the tab, if any.
     */
    private void syncChildTab(Tab tab, @Nullable Token oldTabGroupId) {
        int srcIndex = mModelList.indexFromTabId(tab.getId());

        Token newTabGroupId = tab.getTabGroupId();
        if (oldTabGroupId == null && srcIndex != TabModel.INVALID_TAB_INDEX) {
            oldTabGroupId = mModelList.get(srcIndex).model.get(TabProperties.TAB_GROUP_ID);
        }

        int desIndex = mMediator.getInsertionIndexOfTabForNestedLayout(tab);

        if (srcIndex == TabModel.INVALID_TAB_INDEX && desIndex != TabModel.INVALID_TAB_INDEX) {
            // Tab is moving out of a collapsed group.
            TabModel tabModel = mMediator.getCurrentTabModelChecked();
            int currentTabId = TabModelUtils.getCurrentTabId(tabModel);
            mMediator.addTabInfoToModelForTab(tab, desIndex, currentTabId == tab.getId());
        } else if (srcIndex != TabModel.INVALID_TAB_INDEX
                && desIndex == TabModel.INVALID_TAB_INDEX) {
            // Tab is moving into a collapsed group.
            mModelList.removeAt(srcIndex);
        } else if (srcIndex != TabModel.INVALID_TAB_INDEX) {
            PropertyModel model = mModelList.get(srcIndex).model;
            mMediator.setupGroupPropertiesForChildTab(tab, model);
            mMediator.bindTabActionStateProperties(mMediator.getTabActionState(), tab, model);

            mModelList.moveItem(srcIndex, desIndex);

            if (newTabGroupId != null) {
                int newTabUiIndex = mModelList.indexFromTabId(tab.getId());
                mMediator.ensureGroupHeaderExistsInNestedLayout(tab, newTabGroupId, newTabUiIndex);
            }
        }

        if (newTabGroupId != null) {
            mMediator.updateTabGroupTitle(newTabGroupId);
        }
        if (oldTabGroupId != null && !oldTabGroupId.equals(newTabGroupId)) {
            mMediator.updateTabGroupTitle(oldTabGroupId);
        }
    }

    private void removeChildTabs(Token tabGroupId) {
        int headerIndex = mModelList.indexFromTabGroupId(tabGroupId);
        if (headerIndex == TabModel.INVALID_TAB_INDEX) return;
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        int childCount = tabModel.getTabsInGroup(tabGroupId).size();

        for (int i = 0; i < childCount; i++) {
            if (headerIndex + 1 < mModelList.size()) {
                mModelList.removeAt(headerIndex + 1);
            }
        }
    }

    /**
     * Calculates the target UI index for a moving tab group in a nested layout.
     *
     * @param movedTab The tab that was moved.
     * @param tabModelNewIndex The new backend index of the moved tab.
     * @param relatedTabs The list of tabs in the group being moved.
     * @return The UI index of the element immediately following the group's new position.
     */
    private int getInsertionIndexOfGroupForNestedLayout(
            Tab movedTab, int tabModelNewIndex, List<Tab> relatedTabs) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();

        int offset = relatedTabs.indexOf(movedTab);
        if (offset == -1) return TabModel.INVALID_TAB_INDEX;
        int firstTabIndex = tabModelNewIndex - offset;
        if (firstTabIndex < 0) return TabModel.INVALID_TAB_INDEX;

        int tabAfterIndex = firstTabIndex + relatedTabs.size();
        Tab tabAfter = tabModel.getTabAt(tabAfterIndex);

        if (tabAfter == null) {
            return mModelList.size();
        }

        // If the anchor tab belongs to another group, we must anchor our moving block relative to
        // that group's header card. If it's a standalone tab, we simply map it to its direct UI
        // index.
        Tab tabAfterGroupSelected = TabGroupUtils.getSelectedTabInGroupForTab(tabModel, tabAfter);
        Token tabAfterGroupId = tabAfterGroupSelected.getTabGroupId();
        if (tabAfterGroupId != null) {
            return mModelList.indexFromTabGroupId(tabAfterGroupId);
        } else {
            return mModelList.indexFromTabId(tabAfterGroupSelected.getId());
        }
    }

    /**
     * Updates the UI properties of child tabs when their group color changes.
     *
     * @param tabGroupId The ID of the tab group.
     * @param newColor The new color of the tab group.
     */
    private void updateColorForChildTabsInNestedLayout(
            Token tabGroupId, @TabGroupColorId int newColor) {
        EitherGroupId eitherGroupId = EitherGroupId.createLocalId(new LocalTabGroupId(tabGroupId));
        boolean foundGroup = false;
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel childModel = mModelList.get(i).model;
            if (TabProperties.isTabInGroup(childModel)
                    && tabGroupId.equals(childModel.get(TabProperties.TAB_GROUP_ID))) {
                mMediator.updateTabGroupColorViewProvider(eitherGroupId, childModel, newColor);
                foundGroup = true;
            } else if (foundGroup) {
                break;
            }
        }
    }
}
