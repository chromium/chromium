// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.isOnlyArchivedMsg;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.Pair;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGridAccessibilityHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tabs.TabAlert;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Abstract delegate handler for {@link TabGroupObserver} and {@link TabObserver} callbacks.
 * Layout-specific subclasses override only the callbacks they handle.
 */
@NullMarked
abstract class TabListLayoutDelegate implements TabGroupObserver, TabObserver {
    protected final TabListMediator mMediator;
    protected final TabListModel mModelList;
    private @Nullable TabGridAccessibilityHelper mAccessibilityHelper;

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
     * Whether this layout supports displaying tab groups (e.g. as group cards in GTS or group
     * headers in Vertical Tabs). False for layouts that ignore tab groups (like Flat layout).
     */
    abstract boolean supportsTabGroups();

    /**
     * Whether a child tab in a tab group is represented by a group card in the UI.
     *
     * @param tab The {@link Tab} to check.
     * @return Whether the tab is represented by a group card.
     */
    abstract boolean isChildTabRepresentedByGroupCard(Tab tab);

    /**
     * Resolves the visual alert state indicator (e.g. playing audio) for a tab card or group
     * header.
     *
     * @param representativeTab The representative tab for the card.
     * @param model The property model associated with the tab or group header.
     * @return The {@link TabAlert} that should be displayed, or {@link TabAlert#NONE} if none.
     */
    abstract @TabAlert int getAlertState(Tab representativeTab, PropertyModel model);

    /** Returns the insertion index for a new tab card. */
    abstract int getInsertionIndexOfTab(Tab tab);

    /**
     * Returns the index in {@link #mModelList} of the group with {@code tabGroupId} and the {@link
     * Tab} representing the group. Will be null if the entry is not present, the tab cannot be
     * found, or the tab is not part of a tab group.
     */
    abstract @Nullable Pair<Integer, Tab> getIndexAndTabForTabGroupId(@Nullable Token tabGroupId);

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
     * Handles UI model updates when a tab closure is undone in the tab model.
     *
     * @param tab The {@link Tab} whose closure was undone.
     */
    void tabClosureUndone(Tab tab) {
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
     * Records user action metrics when a tab item is clicked in the UI list.
     *
     * <p>Subclasses can override to customize or suppress metrics (e.g. {@link
     * GroupedLayoutDelegate}).
     *
     * @param tabId The ID of the tab that was selected.
     */
    void recordTabSelection(int tabId) {
        Tab tab = mMediator.getCurrentTabModelChecked().getTabById(tabId);
        if (tab != null
                && tab.getIsPinned()
                && mMediator.getComponentId() == TabComponentId.VERTICAL_TABS) {
            RecordUserAction.record("MobileTabSwitched.VerticalTabsPinned");
        } else {
            RecordUserAction.record(
                    "MobileTabSwitched."
                            + TabUiMetricsHelper.getComponentNameForMetrics(
                                    mMediator.getComponentId()));
        }
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

    // TabObserver implementation.

    @Override
    public void onDidStartNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigationHandle) {
        assert mMediator.isShowingTabs();

        // The URL of the tab and the navigation handle can match without it being a
        // same document navigation if the tab had no renderer and needed to start a
        // new one.
        // See https://crbug.com/40862141.
        if (navigationHandle.isSameDocument()
                || UrlUtilities.isNtpUrl(tab.getUrl())
                || tab.getUrl().equals(navigationHandle.getUrl())) {
            return;
        }
        @Nullable PropertyModel model = mModelList.getModelFromTabId(tab.getId());
        if (model == null || isChildTabRepresentedByGroupCard(tab)) {
            return;
        }

        model.set(
                TabProperties.FAVICON_FETCHER,
                mMediator.getDefaultFaviconFetcher(tab.isIncognito()));
    }

    @Override
    public void onTitleUpdated(Tab updatedTab) {
        assert mMediator.isShowingTabs();

        @Nullable PropertyModel model = mModelList.getModelFromTabId(updatedTab.getId());
        // TODO(crbug.com/40136874) The null check for tab here should be redundant once
        // we have resolved the bug.
        if (model == null
                || mMediator.getCurrentTabModelChecked().getTabById(updatedTab.getId()) == null) {
            return;
        }
        model.set(
                TabProperties.TITLE,
                mMediator.getLatestTitleForTabOrGroup(updatedTab, model, /* useDefault= */ true));
    }

    @Override
    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
        assert mMediator.isShowingTabs();
        if (!toDifferentDocument) return;
        updateLoadingState(tab, true);
    }

    @Override
    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
        assert mMediator.isShowingTabs();
        if (!toDifferentDocument) return;
        updateLoadingState(tab, false);
    }

    @Override
    public void onCrash(Tab tab) {
        assert mMediator.isShowingTabs();
        updateLoadingState(tab, false);
    }

    /**
     * Updates the favicon for a tab or its representing card when the favicon changes.
     *
     * @param updatedTab The {@link Tab} whose favicon was updated.
     * @param icon The updated favicon {@link Bitmap}, or null.
     * @param iconUrl The {@link GURL} of the updated favicon, or null.
     */
    @Override
    public void onFaviconUpdated(Tab updatedTab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
        assert mMediator.isShowingTabs();

        @Nullable PropertyModel model = mModelList.getModelFromTabId(updatedTab.getId());
        if (model == null) return;
        mMediator.updateFaviconForTab(model, updatedTab, icon, iconUrl);
    }

    /**
     * Updates the URL domain, thumbnail, and favicon for a tab or its representing card when its
     * URL changes.
     *
     * @param updatedTab The {@link Tab} whose URL changed.
     */
    @Override
    public void onUrlUpdated(Tab updatedTab) {
        assert mMediator.isShowingTabs();

        @Nullable PropertyModel model = mModelList.getModelFromTabId(updatedTab.getId());
        if (!TabUtils.isValid(updatedTab) || model == null) return;

        model.set(TabProperties.URL_DOMAIN, mMediator.getDomainForTab(updatedTab, model));
        mMediator.updateThumbnailFetcher(model, updatedTab.getId());
        mMediator.updateFaviconForTab(model, updatedTab, null, null);
    }

    /**
     * Updates the alert state indicator for a tab or its representing card when alert state
     * changes.
     *
     * @param updatedTab The {@link Tab} whose alert state changed.
     * @param alertState The new {@link TabAlert} state.
     */
    @Override
    public void onAlertStateChanged(Tab updatedTab, @TabAlert int alertState) {
        assert mMediator.isShowingTabs();

        @Nullable PropertyModel model = mModelList.getModelFromTabId(updatedTab.getId());
        if (model == null || model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION)) {
            return;
        }
        @TabAlert int alertStateToSet = getAlertState(updatedTab, model);
        model.set(TabProperties.ALERT_STATE, alertStateToSet);
        if (model.containsKey(TabProperties.MEDIA_INDICATOR)) {
            model.set(
                    TabProperties.MEDIA_INDICATOR, TabUtils.getMediaStateForAlert(alertStateToSet));
        }
    }

    @Override
    public void onTabPinnedStateChanged(Tab tab, boolean isPinned) {
        assert mMediator.isShowingTabs();

        int index = mModelList.indexFromTabId(tab.getId());
        if (index == TabModel.INVALID_TAB_INDEX) return;

        // When pinning a tab in a group it will be removed from the group so the index
        // update is unnecessary.
        if (!supportsTabGroups()) {
            mMediator.updateTab(index, tab, /* isUpdatingId= */ false, /* quickMode= */ false);
            return;
        }

        int finalIndex =
                mModelList.indexOfNthTabCard(mMediator.getCurrentTabModelChecked().indexOf(tab));
        if (finalIndex == TabModel.INVALID_TAB_INDEX) return;

        ListItem item = mModelList.get(index);
        mModelList.removeAt(index);

        // indexOfNthTabCard returns n + 1 if the index is higher than the number of
        // tabs in the model list.
        // The last valid index to add to is the size of the model list after the
        // removal so we need to clamp to the current size of mModelList.
        finalIndex = Math.min(finalIndex, mModelList.size());
        // Update properties while the item is detached to avoid temporary view type
        // mismatch in the adapter and double-notifications (change + remove).
        mMediator.updateTab(
                item.model, finalIndex, tab, /* isUpdatingId= */ false, /* quickMode= */ false);
        mModelList.add(finalIndex, item);
    }

    /**
     * Handles layout-specific UI model updates when a tab's Actor UI state changes.
     *
     * @param updatedTab The {@link Tab} whose Actor UI state changed.
     * @param state The new {@link UiTabState}.
     */
    void onUiTabStateChanged(Tab updatedTab, UiTabState state) {}

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

    // TabGroupObserver implementation.

    @Override
    public void didChangeTabGroupTitle(Token tabGroupId, String newTitle) {
        mMediator.updateTabGroupTitle(tabGroupId);
    }

    @Override
    public void didMoveWithinGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        if (tabModelNewIndex == tabModelOldIndex) return;

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
     * Returns the {@link ModelType} for tab group cards in this layout. Flat layouts do not have
     * tab groups and use {@link ModelType#TAB}.
     */
    @ModelType
    int getGroupCardType() {
        return ModelType.TAB;
    }

    /**
     * Returns whether the tab group is collapsed in this layout. Flat layouts do not have tab
     * groups and default to true.
     *
     * @param tabGroupId The {@link Token} identifying the tab group.
     */
    boolean isGroupCollapsed(Token tabGroupId) {
        return true;
    }

    /**
     * Called when a tab or group card's selection state is toggled in multi-select mode.
     *
     * @param model The {@link PropertyModel} of the toggled card.
     * @param tabId The ID of the tab associated with the card.
     * @param wasSelected Whether the card was selected prior to the toggle.
     */
    void onTabSelectionToggled(PropertyModel model, int tabId, boolean wasSelected) {}

    /**
     * Returns whether an existing card representing {@code previousTabId} and {@code newTab} are in
     * the same tab group represented by this card, allowing the card's tab ID to be updated in
     * place rather than resetting the list. Flat and nested layouts do not share cards across group
     * tabs and default to false.
     *
     * @param previousTabId The ID of the tab currently represented by the model.
     * @param newTab The incoming {@link Tab} to be displayed at this position.
     * @return Whether the two tabs belong to the same group card in this layout.
     */
    boolean areTabsInSameGroup(int previousTabId, Tab newTab) {
        return false;
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
    protected static int adjustIndexForTabMovement(int currentIndex, int targetTabCurrentIndex) {
        if (targetTabCurrentIndex != TabModel.INVALID_TAB_INDEX
                && currentIndex > targetTabCurrentIndex) {
            return currentIndex - 1;
        }
        return currentIndex;
    }

    /**
     * Sets the accessibility helper used to resolve layout-specific accessibility actions.
     *
     * @param helper The {@link TabGridAccessibilityHelper} instance.
     */
    void setAccessibilityHelper(@Nullable TabGridAccessibilityHelper helper) {
        mAccessibilityHelper = helper;
    }

    /**
     * Allows layout-specific customization of accessibility node info for a given view model.
     *
     * @param host The host view being initialized.
     * @param info The {@link AccessibilityNodeInfo} being populated.
     * @param model The {@link PropertyModel} associated with the view.
     */
    void populateAccessibilityNodeInfo(
            View host, AccessibilityNodeInfo info, @Nullable PropertyModel model) {
        if (mAccessibilityHelper == null
                || model == null
                || !TabProperties.isTabOrTabGroup(model)) {
            return;
        }
        for (AccessibilityAction action : mAccessibilityHelper.getPotentialActionsForView(host)) {
            Pair<Integer, Integer> positions =
                    mAccessibilityHelper.getPositionsOfReorderAction(host, action.getId());
            if (positions != null
                    && positions.first != null
                    && positions.second != null
                    && canReorderToPosition(positions.first, positions.second)) {
                info.addAction(action);
            }
        }
    }

    /**
     * Handles layout-specific accessibility actions.
     *
     * @param host The host view executing the action.
     * @param action The accessibility action ID.
     * @param args Optional bundle arguments.
     * @param model The {@link PropertyModel} associated with the view.
     * @return True if the action was handled, false otherwise.
     */
    boolean performAccessibilityAction(
            View host, int action, @Nullable Bundle args, @Nullable PropertyModel model) {
        if (mAccessibilityHelper != null && mAccessibilityHelper.isReorderAction(action)) {
            Pair<Integer, Integer> positions =
                    mAccessibilityHelper.getPositionsOfReorderAction(host, action);
            if (positions == null
                    || positions.first == null
                    || positions.second == null
                    || !canReorderToPosition(positions.first, positions.second)) {
                return false;
            }
            mModelList.move(positions.first, positions.second);
            RecordUserAction.record("TabGrid.AccessibilityDelegate.Reordered");
            return true;
        }
        return false;
    }

    /**
     * Returns whether a card at {@code sourceIndex} can be reordered to {@code targetIndex}.
     *
     * <p>Reordering is valid if both indices are valid, both cards represent a tab or tab group,
     * and their pinned status matches.
     *
     * @param sourceIndex The index of the item being moved.
     * @param targetIndex The target index where the item would be moved.
     * @return True if reordering between the two positions is valid, false otherwise.
     */
    boolean canReorderToPosition(int sourceIndex, int targetIndex) {
        if (sourceIndex == targetIndex
                || !mModelList.isValidIndex(sourceIndex)
                || !mModelList.isValidIndex(targetIndex)) {
            return false;
        }
        PropertyModel sourceModel = mModelList.get(sourceIndex).model;
        PropertyModel targetModel = mModelList.get(targetIndex).model;
        if (sourceModel == null
                || targetModel == null
                || !TabProperties.isTabOrTabGroup(sourceModel)
                || !TabProperties.isTabOrTabGroup(targetModel)) {
            return false;
        }
        return TabProperties.isPinnedTab(sourceModel) == TabProperties.isPinnedTab(targetModel);
    }

    private void updateLoadingState(Tab tab, boolean isLoading) {
        if (!mMediator.supportsTabLoadingState() || !mMediator.isShowingTabs()) return;
        @Nullable PropertyModel model = mModelList.getModelFromTabId(tab.getId());
        if (model == null) return;
        // Suppress loading indicator for NTP. NTP loads instantly, but the brief load events can
        // trigger visible flickers in Android Views, or get stuck if background tab loading is
        // deferred.
        boolean shouldShowLoadingIndicator = !UrlUtilities.isNtpUrl(tab.getUrl()) && isLoading;
        model.set(TabProperties.IS_LOADING, shouldShowLoadingIndicator);
    }
}
