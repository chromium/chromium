// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.util.Pair;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Objects;

/**
 * {@link TabListMediator.TabListLayoutType#FLAT} implementation of {@link TabListLayoutDelegate}.
 */
@NullMarked
class FlatLayoutDelegate extends TabListLayoutDelegate {
    FlatLayoutDelegate(TabListMediator mediator, TabListModel modelList) {
        super(mediator, modelList);
    }

    @Override
    boolean requiresThumbnailUpdateOnDeselect() {
        return false;
    }

    @Override
    boolean requiresThumbnailUpdateOnSelect() {
        return true;
    }

    @Override
    boolean supportsTabGroups() {
        return false;
    }

    @Override
    boolean isChildTabRepresentedByGroupCard(Tab tab) {
        return false;
    }

    @Override
    @TabAlert
    int getAlertState(Tab representativeTab, PropertyModel model) {
        return representativeTab.getAlertState();
    }

    @Override
    int getInsertionIndexOfTab(Tab tab) {
        if (tab == null) return TabList.INVALID_TAB_INDEX;
        // Compute the index of the tab within the tab's group.
        @Nullable PropertyModel model = mModelList.getFirstTabPropertyModel();
        if (model == null) return TabList.INVALID_TAB_INDEX;

        List<Tab> related = mMediator.getRelatedTabsForId(model.get(TabProperties.TAB_ID));
        int tabIndex = related.indexOf(tab);

        // Get the position of the nth tab card ignoring any other CARD_TYPE entries present in the
        // model list outside of TAB, TAB_GROUP, and ARCHIVED_TAB_GROUP.
        return mModelList.indexOfNthTabCard(tabIndex);
    }

    @Override
    @Nullable Pair<Integer, Tab> getIndexAndTabForTabGroupId(@Nullable Token tabGroupId) {
        return null;
    }

    @Override
    void didMoveTab(Tab tab, int newIndex, int curIndex) {
        // Flat layout does not need to explicitly sync standalone tab moves triggered from
        // external sources to the ModelList.
    }

    // TabGroupObserver implementation.

    @Override
    public void didChangeTabGroupTitle(Token tabGroupId, String newTitle) {
        // No update needed. Flat layout does not display tab group headers.
    }

    @Override
    public void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {
        // No update needed. Flat layout does not display tab group headers.
    }

    @Override
    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab previousGroupTab = tabModel.getRepresentativeTabAt(prevFilterIndex);
        assumeNonNull(previousGroupTab);

        int previousGroupTabId = previousGroupTab.getId();
        int movedTabId = movedTab.getId();
        int previousTabListModelIndex = mModelList.indexFromTabId(previousGroupTabId);
        // Invalid means the previous group tab isn't visible. Either:
        // 1. The moved tab isn't in this model list.
        // 2. The moved tab is meant to stay in the model list as this is the
        //    destination group.
        // In either case no-op.
        if (previousTabListModelIndex == TabList.INVALID_TAB_INDEX) {
            return;
        }

        // The moved tab isn't here, or it is out-of-bounds no-op.
        int curTabListModelIndex = mModelList.indexFromTabId(movedTabId);
        if (!mModelList.isValidIndex(curTabListModelIndex)) return;

        mModelList.removeAt(curTabListModelIndex);
    }

    @Override
    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        // If no tab is present we can't check if the added tab is part of the
        // current group. Assume it isn't since a group state with 0 tab should be
        // impossible.
        @Nullable PropertyModel model = mModelList.getFirstTabPropertyModel();
        if (model == null) return;

        // If the added tab is part of the group add it and update the dialog.
        int firstTabId = model.get(TabProperties.TAB_ID);
        Tab firstTab = tabModel.getTabById(firstTabId);
        if (firstTab == null
                || !Objects.equals(firstTab.getTabGroupId(), movedTab.getTabGroupId())) {
            return;
        }

        mMediator.addObserversForTab(movedTab);
        onTabAdded(movedTab);
    }
}
