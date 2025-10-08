// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.ALL_KEYS_TAB_GRID;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.FAVICON_FETCHER;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.GRID_CARD_SIZE;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_PINNED;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TITLE;

import static java.lang.Math.max;

import android.content.Context;
import android.content.res.Resources;
import android.util.Size;

import androidx.annotation.Px;
import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.base.Callback;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Mediator for the pinned tabs strip. This class is the business logic controller for the pinned
 * tabs strip, responsible for observing the main tab grid and updating the model of the pinned tabs
 * strip accordingly. It is a part of the MVC pattern with the {@link PinnedTabStripCoordinator}.
 */
@NullMarked
public class PinnedTabStripMediator {

    private final Context mContext;
    private final TabListModel mTabGridListModel;
    private final TabListCoordinator mTabLisCoordinator;
    private final TabListModel mPinnedTabsModelList;
    private final GridLayoutManager mTabGridListLayoutManager;
    private final PropertyModel mStripPropertyModel;
    private final TabListItemSizeChangedObserver mTabListItemSizeChangedObserver;
    private final TabModelObserver mTabModelObserver;
    private final ObservableSupplier<@Nullable TabGroupModelFilter> mTabGroupModelFilterSupplier;

    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);

    /**
     * The current width of a tab list item in the main tab grid. This is used to calculate the
     * width of pinned tabs in the strip.
     */
    private @Px int mTabListItemCurrentWidth;

    /**
     * Constructor for the PinnedTabsStripMediator.
     *
     * @param context The current context for getting the required reources.
     * @param tabGridListLayoutManager The layout manager for the main tab grid.
     * @param tabListCoordinator The coordinator for the main tab grid.
     * @param tabGridListModel The model for the main tab grid.
     * @param pinnedTabsModelList The model for the pinned tabs strip.
     * @param stripPropertyModel The property model for the pinned tabs strip.
     * @param tabGroupModelFilterSupplier The supplier of the current {@link TabGroupModelFilter}.
     */
    public PinnedTabStripMediator(
            Context context,
            GridLayoutManager tabGridListLayoutManager,
            TabListCoordinator tabListCoordinator,
            TabListModel tabGridListModel,
            TabListModel pinnedTabsModelList,
            PropertyModel stripPropertyModel,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mContext = context;
        mTabGridListLayoutManager = tabGridListLayoutManager;
        mTabGridListModel = tabGridListModel;
        mPinnedTabsModelList = pinnedTabsModelList;
        mStripPropertyModel = stripPropertyModel;
        mTabLisCoordinator = tabListCoordinator;
        mTabListItemSizeChangedObserver = this::onTabGridListItemSizeChanged;
        mTabLisCoordinator.addTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
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
                };
        mTabGroupModelFilterSupplier.addSyncObserverAndCallIfNonNull(mOnTabGroupModelFilterChanged);
    }

    /**
     * The main entry point for updates. Called when the main tab grid is scrolled, which may
     * trigger a change in the set of visible pinned tabs.
     */
    public void onScrolled() {
        updatePinnedTabsBar();
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
        List<ListItem> newPinnedTabs = new ArrayList<>();

        // Find pinned tabs that are scrolled off-screen (above the current viewport).
        for (int i = 0; i < mTabGridListModel.size() && i < firstVisiblePosition; i++) {
            ListItem item = mTabGridListModel.get(i);
            if (item == null) continue;

            PropertyModel model = item.model;
            if (model.get(TabListModel.CardProperties.CARD_TYPE)
                    != TabListModel.CardProperties.ModelType.TAB) {
                continue;
            }

            if (model.get(IS_PINNED)) {
                newPinnedTabs.add(createPinnedTabListItem(model));
            }
        }
        return newPinnedTabs;
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
        Size pinnedTabSize = new Size(mTabListItemCurrentWidth, 0);

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
        mStripPropertyModel.set(
                PinnedTabStripProperties.SCROLL_TO_POSITION, mPinnedTabsModelList.size() - 1);
    }

    private void onTabGridListItemSizeChanged(int spanCount, Size cardSize) {
        // TODO(crbug.com/444221209): Find better way to handle this to avoid lot of unnecessary
        // resource fetch calls.
        @Px
        int delta =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.pinned_tab_strip_item_width_delta);
        mTabListItemCurrentWidth = cardSize.getWidth() - delta;
        onPinnedTabStripItemWidthChanged();
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

        Resources res = mContext.getResources();
        @Px int minAllowedWidth = PinnedTabStripUtils.getMinAllowedWidthForPinTabStripItemPx(res);
        float widthPercentage =
                PinnedTabStripUtils.getWidthPercentageMultiplier(
                        res, mTabGridListLayoutManager, mPinnedTabsModelList.size());

        int newWidth = Math.round(mTabListItemCurrentWidth * widthPercentage);
        Size newSize = new Size(max(minAllowedWidth, newWidth), 0);
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
        if (oldFilter != null) {
            oldFilter.removeObserver(mTabModelObserver);
        }
        if (newFilter != null) {
            newFilter.addObserver(mTabModelObserver);
        }
    }

    void destroy() {
        mTabLisCoordinator.removeTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
        mTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
    }
}
