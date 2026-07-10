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
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Abstract delegate handler for {@link TabGroupObserver} callbacks. Layout-specific subclasses
 * override only the callbacks they handle.
 */
@NullMarked
abstract class TabListLayoutDelegate implements TabGroupObserver {
    protected final TabListMediator mMediator;
    protected final TabListModel mModelList;

    TabListLayoutDelegate(TabListMediator mediator, TabListModel modelList) {
        mMediator = mediator;
        mModelList = modelList;
    }

    /** Returns the insertion index for a new tab card. */
    public abstract int getInsertionIndexOfTab(Tab tab);

    @Override
    public void didChangeTabGroupTitle(Token tabGroupId, String newTitle) {
        mMediator.updateTabGroupTitle(tabGroupId);
    }

    @Override
    public void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {
        @Nullable Pair<Integer, Tab> indexAndTab =
                mMediator.getIndexAndTabForTabGroupId(tabGroupId);
        if (indexAndTab == null) return;
        Tab tab = indexAndTab.second;
        PropertyModel model = mModelList.get(indexAndTab.first).model;

        mMediator.updateTabGroupProperties(tab, model, newColor);
        mMediator.updateFaviconForTab(model, tab, null, null);
        mMediator.updateDescriptionString(tab, model);
        mMediator.updateActionButtonDescriptionString(tab, model);
        mMediator.updateThumbnailFetcher(model, tab.getId());
    }

    @Override
    public void didMoveWithinGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();

        // Maintain correct order.
        int curPosition = mModelList.indexFromTabId(movedTab.getId());

        if (!mModelList.isValidIndex(curPosition)) return;

        Tab destinationTab =
                tabModel.getTabAt(
                        tabModelNewIndex > tabModelOldIndex
                                ? tabModelNewIndex - 1
                                : tabModelNewIndex + 1);
        assumeNonNull(destinationTab);
        int newPosition = mModelList.indexFromTabId(destinationTab.getId());

        mModelList.moveItem(curPosition, newPosition);
    }

    /**
     * Initializes layout-specific group properties on a child tab card model. Defaults to a no-op
     * for layouts that do not style child tab cards.
     */
    public void setupGroupPropertiesForChildTab(Tab tab, PropertyModel model) {}

    protected boolean hasHigherBackendIndex(int modelIndex, int targetModelIndex) {
        return modelIndex != TabModel.INVALID_TAB_INDEX && modelIndex > targetModelIndex;
    }

    /**
     * Adjusts the proposed insertion UI index if the tab is being moved from an earlier position.
     *
     * <p>If a tab is already present in the UI list (meaning it is being moved rather than newly
     * inserted) and its current UI index is less than the proposed insertion index, removing the
     * tab from its old position will shift all subsequent UI indices down by one. We must decrement
     * the insertion index by one to account for this shift.
     *
     * @param currentIndex The proposed insertion UI index.
     * @param targetTabCurrentIndex The current UI index of the tab being moved, or
     *     TabModel.INVALID_TAB_INDEX if the tab is not currently in the UI list.
     * @return The adjusted insertion UI index.
     */
    protected int adjustIndexForTabMovement(int currentIndex, int targetTabCurrentIndex) {
        if (targetTabCurrentIndex != TabModel.INVALID_TAB_INDEX
                && currentIndex > targetTabCurrentIndex) {
            return currentIndex - 1;
        }
        return currentIndex;
    }
}
