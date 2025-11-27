// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.ALL_KEYS_TAB_GRID;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.FAVICON_FETCHER;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.GRID_CARD_SIZE;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_PINNED;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_CONTEXT_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TITLE;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.IS_VISIBLE;

import static java.lang.Math.max;

import android.app.Activity;
import android.content.res.Resources;
import android.util.Size;
import android.view.View;

import androidx.annotation.Px;
import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.CancelLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabGridViewRectUpdater;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
import java.util.List;

/**
 * Mediator for the pinned tabs strip. This class is the business logic controller for the pinned
 * tabs strip, responsible for observing the main tab grid and updating the model of the pinned tabs
 * strip accordingly. It is a part of the MVC pattern with the {@link PinnedTabStripCoordinator}.
 */
@NullMarked
public class PinnedTabStripMediator {

    private final Activity mActivity;
    private final TabListModel mTabGridListModel;
    private final TabListCoordinator mTabLisCoordinator;
    private final TabListModel mPinnedTabsModelList;
    private final GridLayoutManager mTabGridListLayoutManager;
    private final PropertyModel mStripPropertyModel;
    private final TabListItemSizeChangedObserver mTabListItemSizeChangedObserver;
    private final TabModelObserver mTabModelObserver;
    private final ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final ObservableSupplier<@Nullable TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private @Nullable PinnedTabStripItemContextMenuCoordinator mContextMenuCoordinator;
    private final BottomSheetController mBottomSheetController;
    private @Nullable TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    private final ModalDialogManager mModalDialogManager;
    private final @Nullable Runnable mOnTabGroupCreation;
    private final @Px int mPinnedTabListItemHeight;
    private final @Px int mPinnedTabsStripRowCoverageHeightPx;

    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private final TabActionListener mContextClickTabItemEventListener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    onLongPress(tabId, view);
                }

                @Override
                public void run(
                        View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                    // No-op.
                }
            };

    /**
     * The current width of a tab list item in the main tab grid. This is used to calculate the
     * width of pinned tabs in the strip.
     */
    private @Px int mTabListItemCurrentWidth;

    /**
     * Constructor for the PinnedTabsStripMediator.
     *
     * @param activity The current activity for getting the required resources.
     * @param tabGridListLayoutManager The layout manager for the main tab grid.
     * @param tabListCoordinator The coordinator for the main tab grid.
     * @param tabGridListModel The model for the main tab grid.
     * @param pinnedTabsModelList The model for the pinned tabs strip.
     * @param stripPropertyModel The property model for the pinned tabs strip.
     * @param tabGroupModelFilterSupplier The supplier of the current {@link TabGroupModelFilter}.
     */
    public PinnedTabStripMediator(
            Activity activity,
            GridLayoutManager tabGridListLayoutManager,
            TabListCoordinator tabListCoordinator,
            TabListModel tabGridListModel,
            TabListModel pinnedTabsModelList,
            PropertyModel stripPropertyModel,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            @Nullable Runnable onTabGroupCreation) {
        mActivity = activity;
        mTabGridListLayoutManager = tabGridListLayoutManager;
        mTabGridListModel = tabGridListModel;
        mPinnedTabsModelList = pinnedTabsModelList;
        mStripPropertyModel = stripPropertyModel;
        mTabLisCoordinator = tabListCoordinator;
        mTabListItemSizeChangedObserver = this::onTabGridListItemSizeChanged;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mOnTabGroupCreation = onTabGroupCreation;
        mTabLisCoordinator.addTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        Resources res = mActivity.getResources();
        mPinnedTabListItemHeight = res.getDimensionPixelSize(R.dimen.pinned_tab_strip_item_height);
        mPinnedTabsStripRowCoverageHeightPx =
                res.getDimensionPixelSize(R.dimen.pinned_tabs_strip_row_coverage_height);
        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        int oldIndex = mPinnedTabsModelList.indexFromTabId(lastId);
                        if (oldIndex != TabModel.INVALID_TAB_INDEX) {
                            mPinnedTabsModelList.get(oldIndex).model.set(IS_SELECTED, false);
                        }
                        int newIndex = mPinnedTabsModelList.indexFromTabId(tab.getId());
                        if (newIndex != TabModel.INVALID_TAB_INDEX) {
                            mPinnedTabsModelList.get(newIndex).model.set(IS_SELECTED, true);
                        }
                    }

                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs, boolean isAllTabs, int closingSource) {
                        updatePinnedTabsBar();
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        // We rely on tab list model for updating the pinned tabs, which could be
                        // updated async when the observers are fired during undone. Hence we post
                        // the task to queue to trigger the UI update correctly.
                        ThreadUtils.postOnUiThread(() -> updatePinnedTabsBar());
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        updatePinnedTabsBar();
                    }

                    @Override
                    public void didChangePinState(Tab tab) {
                        updatePinnedTabsBar();
                    }
                };
        mTabGroupModelFilterSupplier.addSyncObserverAndCallIfNonNull(mOnTabGroupModelFilterChanged);
    }

    /**
     * Called when a long press is detected on a pinned tab.
     *
     * @param tabId The id of the tab that was long pressed.
     * @param view The view that was long pressed.
     * @return A {@link CancelLongPressTabItemEventListener} to be notified when the long press is
     *     cancelled.
     */
    @Nullable CancelLongPressTabItemEventListener onLongPress(
            @TabId int tabId, @Nullable View view) {
        if (view == null || mContextMenuCoordinator == null) return null;
        ViewRectProvider viewRectProvider = new ViewRectProvider(view, TabGridViewRectUpdater::new);
        mContextMenuCoordinator.showMenu(viewRectProvider, tabId);
        return mContextMenuCoordinator::dismiss;
    }

    /**
     * The main entry point for updates. Called when the main tab grid is scrolled, which may
     * trigger a change in the set of visible pinned tabs.
     */
    void onScrolled() {
        updatePinnedTabsBar();
    }

    /** Returns whether the pinned tab strip is currently visible. */
    boolean isPinnedTabsBarVisible() {
        return mStripPropertyModel.get(IS_VISIBLE);
    }

    /**
     * Main orchestration method. Checks if the pinned tabs strip needs to be updated and triggers
     * the model and visibility updates if necessary.
     */
    private void updatePinnedTabsBar() {
        List<ListItem> newPinnedTabs = getVisiblePinnedTabs();
        if (PinnedTabStripUtils.areListsOfTabsSame(newPinnedTabs, mPinnedTabsModelList)) {
            return;
        }

        updatePinnedTabsModel(newPinnedTabs);
        updateStripVisibility();
    }

    /**
     * Determines which pinned tabs should be visible in the strip.
     *
     * @return A list of {@link ListItem}s representing the pinned tabs that are currently scrolled
     *     off-screen (above the viewport) in the main tab grid.
     */
    private List<ListItem> getVisiblePinnedTabs() {
        if (mTabGridListLayoutManager == null) return new ArrayList<>();

        int firstVisiblePosition = mTabGridListLayoutManager.findFirstVisibleItemPosition();
        if (firstVisiblePosition < 0) return new ArrayList<>();

        int lastItemToConsiderForPinning =
                maybeIncludePartiallyVisibleRow(
                        firstVisiblePosition, mTabGridListLayoutManager.getSpanCount());

        List<ListItem> newPinnedTabs = new ArrayList<>();

        // Find pinned tabs that are scrolled off-screen (above the current viewport) or are in a
        // partially visible row covered by the pinned tabs strip.
        for (int i = 0; i < lastItemToConsiderForPinning; i++) {
            ListItem item = mTabGridListModel.get(i);
            if (item == null) continue;

            PropertyModel model = item.model;
            if (!isTabItem(model)) {
                continue;
            }

            if (model.get(IS_PINNED)) {
                newPinnedTabs.add(createPinnedTabListItem(model));
            }
        }
        return newPinnedTabs;
    }

    /**
     * Checks if the first visible row in the tab grid is partially covered by the pinned tab strip.
     * If so, it returns an updated index to include the items in that row for pinning
     * consideration.
     *
     * @param firstVisiblePosition The position of the first visible item in the grid.
     * @param spanCount The span count of the grid layout.
     * @return The index of the last item to consider for pinning. This will be an updated index if
     *     the row is covered, otherwise it will be {@code firstVisiblePosition}.
     */
    private int maybeIncludePartiallyVisibleRow(int firstVisiblePosition, int spanCount) {
        int lastItemToConsiderForPinning = firstVisiblePosition;

        // Check if the row exists and is partially covered by the pinned tab strip.
        if (firstVisiblePosition < mTabGridListModel.size()
                && isTabItem(mTabGridListModel.get(firstVisiblePosition).model)) {
            View nextRowFirstItemView =
                    mTabGridListLayoutManager.findViewByPosition(firstVisiblePosition);

            // Visual representation of what is happening when a row is hidden by the pinned tab
            // bar. The condition below checks if the bottom of the first visible tab row (Tab 3
            // and 4 in this diagram) is behind the pinned tab bar, meaning it is covered enough
            // to be pinned.
            // +-----------------------------------+---------------------+
            // | Pinned Tab Bar [---] [---] [---]  |                     |
            // |                                   |                     |
            // |         Tab 3                     |        Tab 4        |
            // + - - - - - - - - - - - - - - - - - + - - - - - - - - - - + <--- nextRowBottom
            // +===================================+=====================+ <--- PinnedBarCoverage
            // | ...                               | ...                 |
            if (nextRowFirstItemView != null
                    && nextRowFirstItemView.getBottom() <= mPinnedTabsStripRowCoverageHeightPx) {
                lastItemToConsiderForPinning = firstVisiblePosition + spanCount;
            }
        }
        return Math.min(lastItemToConsiderForPinning, mTabGridListModel.size());
    }

    /** Returns whether the given model is for a tab item. */
    private boolean isTabItem(PropertyModel model) {
        return model.get(TabListModel.CardProperties.CARD_TYPE)
                == TabListModel.CardProperties.ModelType.TAB;
    }

    /**
     * Creates a new ListItem for the pinned tabs strip from a PropertyModel.
     *
     * @param model The PropertyModel of the tab in the main grid.
     * @return A new ListItem for the pinned tabs strip.
     */
    private ListItem createPinnedTabListItem(PropertyModel model) {
        // The view will animate to its final size, so we can set a default width here.
        // The correct width will be set in resizePinnedTabCards.
        Size pinnedTabSize = new Size(mTabListItemCurrentWidth, mPinnedTabListItemHeight);

        PropertyModel newModel =
                new PropertyModel.Builder(ALL_KEYS_TAB_GRID)
                        .with(TAB_ID, model.get(TAB_ID))
                        .with(TITLE, model.get(TITLE))
                        .with(FAVICON_FETCHER, model.get(FAVICON_FETCHER))
                        .with(IS_SELECTED, model.get(IS_SELECTED))
                        .with(GRID_CARD_SIZE, pinnedTabSize)
                        .with(TAB_CLICK_LISTENER, model.get(TAB_CLICK_LISTENER))
                        .with(
                                TAB_ACTION_BUTTON_DATA,
                                new TabActionButtonData(TabActionButtonType.PIN, null))
                        .with(TAB_CONTEXT_CLICK_LISTENER, mContextClickTabItemEventListener)
                        .with(IS_INCOGNITO, model.get(IS_INCOGNITO))
                        .build();
        return new ListItem(UiType.TAB, newModel);
    }

    /**
     * Updates the pinned tabs model list with a new list of tabs.
     *
     * @param newPinnedTabs The new list of pinned tabs to display.
     */
    private void updatePinnedTabsModel(List<ListItem> newPinnedTabs) {
        // Perform a granular update instead of clear() and addAll() to prevent flashing.
        for (int i = 0; i < newPinnedTabs.size(); i++) {
            ListItem newItem = newPinnedTabs.get(i);

            if (i < mPinnedTabsModelList.size()) {
                if (newItem.model.get(TAB_ID) != mPinnedTabsModelList.get(i).model.get(TAB_ID)) {
                    mPinnedTabsModelList.removeAt(i);
                    mPinnedTabsModelList.add(i, newItem);
                }
            } else {
                mPinnedTabsModelList.add(newItem);
            }
        }

        // Remove any trailing items that are no longer in the new list.
        if (newPinnedTabs.size() < mPinnedTabsModelList.size()) {
            mPinnedTabsModelList.removeRange(
                    newPinnedTabs.size(), mPinnedTabsModelList.size() - newPinnedTabs.size());
        }

        resizePinnedTabCards();

        // We use -1 as a trigger to reset any previously set values to the property.
        int scrollTo = -1;
        if (mPinnedTabsModelList.size() > mTabGridListLayoutManager.getSpanCount()) {
            scrollTo = mPinnedTabsModelList.size() - 1;
        }

        mStripPropertyModel.set(PinnedTabStripProperties.SCROLL_TO_POSITION, scrollTo);
    }

    private void onTabGridListItemSizeChanged(int spanCount, Size cardSize) {
        // TODO(crbug.com/444221209): Find better way to handle this to avoid lot of unnecessary
        // resource fetch calls.
        @Px
        int delta =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.pinned_tab_strip_item_width_delta);
        mTabListItemCurrentWidth = cardSize.getWidth() - delta;
        onPinnedTabStripItemWidthChanged();
        updatePinnedTabsBar();
    }

    private void resizePinnedTabCards() {
        onPinnedTabStripItemWidthChanged();
    }

    /**
     * Updates the width of the cards in the pinned tab strip. Resizes the pinned tab cards based on
     * the available width and the number of pinned tabs. The cards will shrink as more tabs are
     * added to the strip, up to a minimum width. This method calculates the new width percentage
     * based on the current state and applies it to the cards.
     */
    private void onPinnedTabStripItemWidthChanged() {
        if (mPinnedTabsModelList.isEmpty()) return;

        Resources res = mActivity.getResources();
        @Px int minAllowedWidth = PinnedTabStripUtils.getMinAllowedWidthForPinTabStripItemPx(res);
        float widthPercentage =
                PinnedTabStripUtils.getWidthPercentageMultiplier(
                        res, mTabGridListLayoutManager, mPinnedTabsModelList.size());

        int newWidth = Math.round(mTabListItemCurrentWidth * widthPercentage);
        Size newSize = new Size(max(minAllowedWidth, newWidth), mPinnedTabListItemHeight);
        for (ListItem item : mPinnedTabsModelList) {
            item.model.set(GRID_CARD_SIZE, newSize);
        }
    }

    /**
     * Manages the visibility and animations of the pinned tabs strip. Shows the strip with an
     * animation if it's not empty, and hides it otherwise.
     */
    private void updateStripVisibility() {
        boolean shouldBeVisible = !mPinnedTabsModelList.isEmpty();
        mStripPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, shouldBeVisible);
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        if (mTabGroupListBottomSheetCoordinator != null) {
            mTabGroupListBottomSheetCoordinator.destroy();
        }

        if (oldFilter != null) {
            oldFilter.removeObserver(mTabModelObserver);
        }
        if (newFilter != null) {
            newFilter.addObserver(mTabModelObserver);
            Profile profile = mTabGroupModelFilterSupplier.get().getTabModel().getProfile();
            boolean isIncognito = newFilter.getTabModel().isIncognitoBranded();
            assumeNonNull(profile);

            TabGroupCreationDialogManager tabGroupCreationDialogManager =
                    new TabGroupCreationDialogManager(
                            mActivity, mModalDialogManager, mOnTabGroupCreation);
            mTabGroupListBottomSheetCoordinator =
                    new TabGroupListBottomSheetCoordinator(
                            mActivity,
                            profile,
                            tabGroupId ->
                                    tabGroupCreationDialogManager.showDialog(tabGroupId, newFilter),
                            /* tabMovedCallback= */ null,
                            newFilter,
                            mBottomSheetController,
                            /* supportsShowNewGroup= */ true,
                            /* destroyOnHide= */ false);
            mContextMenuCoordinator =
                    PinnedTabStripItemContextMenuCoordinator.createContextMenuCoordinator(
                            mActivity,
                            mTabBookmarkerSupplier,
                            newFilter,
                            mTabGroupListBottomSheetCoordinator,
                            tabGroupCreationDialogManager);

            mStripPropertyModel.set(
                    BACKGROUND_COLOR, ChromeColors.getDefaultBgColor(mActivity, isIncognito));
        }
    }

    void setContextMenuCoordinatorForTesting(PinnedTabStripItemContextMenuCoordinator coordinator) {
        mContextMenuCoordinator = coordinator;
    }

    void destroy() {
        mTabLisCoordinator.removeTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
        mTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
        if (mTabGroupListBottomSheetCoordinator != null) {
            mTabGroupListBottomSheetCoordinator.destroy();
            mTabGroupListBottomSheetCoordinator = null;
        }
    }
}
