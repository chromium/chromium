// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGridDialogHandler;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Objects;

/**
 * {@link TabListMediator.TabListLayoutType#FLAT} implementation of {@link TabListLayoutDelegate}.
 */
@NullMarked
class FlatLayoutDelegate extends TabListLayoutDelegate {
    private final @Nullable TabGridDialogHandler mTabGridDialogHandler;

    FlatLayoutDelegate(
            TabListMediator mediator,
            TabListModel modelList,
            @Nullable TabGridDialogHandler dialogHandler) {
        super(mediator, modelList);
        mTabGridDialogHandler = dialogHandler;
    }

    @Override
    public int getInsertionIndexOfTab(Tab tab) {
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
        if (mTabGridDialogHandler != null) {
            boolean isUngroupingLastTabInGroup = previousGroupTabId == movedTabId;
            mTabGridDialogHandler.updateDialogContent(
                    isUngroupingLastTabInGroup ? Tab.INVALID_TAB_ID : previousGroupTabId);
        }
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
        mMediator.onTabAdded(movedTab);
        if (mTabGridDialogHandler != null) {
            mTabGridDialogHandler.updateDialogContent(
                    tabModel.getGroupLastShownTabId(firstTab.getTabGroupId()));
        }
    }
}
