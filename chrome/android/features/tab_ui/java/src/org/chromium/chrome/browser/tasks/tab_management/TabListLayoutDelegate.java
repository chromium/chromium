// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.isOnlyArchivedMsg;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

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

    /**
     * Whether this layout requires a thumbnail cache invalidation fetch when a tab is deselected.
     * Often required for multi-thumbnail cluster views (like Grouped layouts).
     */
    abstract boolean requiresThumbnailUpdateOnDeselect();

    /**
     * Whether this layout requires fetching a fresh thumbnail when a tab becomes selected. Should
     * be false for layouts that do not support thumbnails (like Vertical Tabs).
     */
    abstract boolean requiresThumbnailUpdateOnSelect();

    /**
     * Resolves the visual media state indicator (e.g. playing audio) for a tab card or group
     * header.
     *
     * @param representativeTab The representative tab for the card.
     * @param model The property model associated with the tab or group header.
     * @return The {@link MediaState} that should be displayed.
     */
    abstract @MediaState int getMediaIndicatorState(Tab representativeTab, PropertyModel model);

    /** Returns the insertion index for a new tab card. */
    abstract int getInsertionIndexOfTab(Tab tab);

    /**
     * Handles tab insertion into {@link #mModelList} by resolving the target insertion index,
     * placing the tab after any leading archived message card, and delegating model creation to
     * {@link TabListMediator#addTabCardToModel}. If the tab is already present in the list, returns
     * its existing index without modifying the model.
     *
     * @param tab The {@link Tab} being added.
     * @return The UI index where the tab was inserted, or {@link TabModel#INVALID_TAB_INDEX} if the
     *     tab was not added to the model list (e.g. child tab of a collapsed group).
     */
    int onTabAdded(Tab tab) {
        int existingIndex = mModelList.indexFromTabId(tab.getId());
        if (existingIndex != TabModel.INVALID_TAB_INDEX) return existingIndex;

        int newIndex = getInsertionIndexOfTab(tab);

        // Tabs should be inserted only after the archived message card.
        if (newIndex == 0 && isOnlyArchivedMsg(mModelList)) newIndex++;

        if (newIndex == TabList.INVALID_TAB_INDEX) return newIndex;

        mMediator.addTabCardToModel(tab, newIndex);
        return newIndex;
    }

    /**
     * Handles UI model updates when a tab is added to the tab model.
     *
     * @param tab The {@link Tab} being added.
     * @param type The {@link TabLaunchType} indicating how the tab was launched.
     */
    void didAddTab(Tab tab, @TabLaunchType int type) {
        onTabAdded(tab);
    }

    /**
     * Resolves the UI index in {@link #mModelList} of the card representing the given tab in this
     * layout.
     *
     * <p>For flat and nested layouts, this locates the tab's direct card in the model list.
     * Subclasses (such as grouped layouts) may override this to resolve to the containing group
     * card if the tab is part of a tab group.
     *
     * @param tabId The ID of the tab to locate.
     * @return The UI index in {@link #mModelList}, or {@link TabModel#INVALID_TAB_INDEX} if not
     *     present.
     */
    int getUiIndexForTab(int tabId) {
        return mModelList.indexFromTabId(tabId);
    }

    /**
     * Handles UI model updates when a tab is selected in the tab model.
     *
     * @param tab The {@link Tab} that was selected.
     * @param type The {@link TabSelectionType} indicating the selection trigger.
     * @param lastId The ID of the previously selected tab.
     */
    void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
        int oldIndex = getUiIndexForTab(lastId);
        int newIndex = getUiIndexForTab(tab.getId());

        mMediator.setLastSelectedTabListModelIndex(oldIndex);
        if (mMediator.isTabDelayed(tab)) {
            // If tab is being added later, it will be selected later.
            return;
        }
        mMediator.selectTab(oldIndex, newIndex);
    }

    /**
     * Updates the favicon for a tab or its representing card when the favicon changes.
     *
     * @param updatedTab The {@link Tab} whose favicon was updated.
     * @param icon The updated favicon {@link Bitmap}, or null.
     * @param iconUrl The {@link GURL} of the updated favicon, or null.
     */
    void onFaviconUpdated(Tab updatedTab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
        @Nullable PropertyModel model = mModelList.getModelFromTabId(updatedTab.getId());
        if (model == null) return;
        mMediator.updateFaviconForTab(model, updatedTab, icon, iconUrl);
    }

    /**
     * Handles UI model updates when a tab is removed for closure.
     *
     * @param tab The {@link Tab} being removed for closure.
     */
    void onTabClose(Tab tab) {
        int index = mModelList.indexFromTabId(tab.getId());
        if (index == TabModel.INVALID_TAB_INDEX) return;

        mModelList.removeAt(index);
    }

    /**
     * Prepares layout-specific view properties and animation tags prior to tab closure animation.
     *
     * @param view The clicked close button {@link View}, or null.
     * @param closingTabIndex The UI index of the tab being closed in {@link #mModelList}.
     */
    void prepareTabCloseAnimation(@Nullable View view, int closingTabIndex) {}

    /**
     * Handles UI model updates when a tab is moved in the tab model.
     *
     * @param tab The {@link Tab} that moved.
     * @param newIndex The new index of the tab in the {@link TabModel}.
     * @param curIndex The previous index of the tab in the {@link TabModel}.
     */
    void didMoveTab(Tab tab, int newIndex, int curIndex) {
        // Standalone tab moves triggered from external sources need to be
        // explicitly synced to the ModelList for GROUPED and NESTED layouts.

        // Intra-group move or merging into group.
        if (tab.getTabGroupId() != null) {
            return;
        }

        int currentUiIndex = mModelList.indexFromTabId(tab.getId());
        if (currentUiIndex == TabModel.INVALID_TAB_INDEX) return;

        // Moving out of a group.
        // This assumes the move event is dispatched before the ungroup event
        // (didMoveTabOutOfGroup) is processed, meaning the UI model still has the
        // old grouping metadata.
        PropertyModel model = mModelList.get(currentUiIndex).model;
        if (TabProperties.isTabInGroup(model) || TabProperties.isTabGroupHeader(model)) {
            return;
        }

        // Standalone tab movement.
        int targetUiIndex = getInsertionIndexOfTab(tab);
        mModelList.moveItem(currentUiIndex, targetUiIndex);
    }

    @Override
    public void didChangeTabGroupTitle(Token tabGroupId, String newTitle) {
        mMediator.updateTabGroupTitle(tabGroupId);
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
     * Configures layout-specific group properties on a child tab card model (e.g. group spine
     * styling in NESTED layouts). Defaults to a no-op in layouts that do not style child tab rows.
     *
     * @param tab The {@link Tab} being configured.
     * @param model The {@link PropertyModel} of the child tab card.
     */
    void setupGroupPropertiesForChildTab(Tab tab, PropertyModel model) {}

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
    protected static int adjustIndexForTabMovement(int currentIndex, int targetTabCurrentIndex) {
        if (targetTabCurrentIndex != TabModel.INVALID_TAB_INDEX
                && currentIndex > targetTabCurrentIndex) {
            return currentIndex - 1;
        }
        return currentIndex;
    }
}
