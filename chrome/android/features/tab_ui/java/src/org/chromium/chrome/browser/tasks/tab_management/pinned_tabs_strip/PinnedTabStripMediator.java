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

import android.app.Activity;
import android.util.Size;

import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
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

    private final Activity mActivity;
    private final TabListModel mTabGridListModel;
    private final TabListModel mPinnedTabsModelList;
    private final GridLayoutManager mTabGridListLayoutManager;
    private final PropertyModel mStripPropertyModel;
    private int mPinnedTabStripItemWidth = -1;

    /**
     * Constructor for the PinnedTabsStripMediator.
     *
     * @param activity The current activity.
     * @param tabGridListLayoutManager The layout manager for the main tab grid.
     * @param tabGridListModel The model for the main tab grid.
     * @param pinnedTabsModelList The model for the pinned tabs strip.
     * @param stripPropertyModel The property model for the pinned tabs strip.
     */
    public PinnedTabStripMediator(
            Activity activity,
            GridLayoutManager tabGridListLayoutManager,
            TabListModel tabGridListModel,
            TabListModel pinnedTabsModelList,
            PropertyModel stripPropertyModel) {
        mActivity = activity;
        mTabGridListLayoutManager = tabGridListLayoutManager;
        mTabGridListModel = tabGridListModel;
        mPinnedTabsModelList = pinnedTabsModelList;
        mStripPropertyModel = stripPropertyModel;
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
        Size pinnedTabSize = new Size(mPinnedTabStripItemWidth, -1);

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

        if (newPinnedTabs.size() < mPinnedTabsModelList.size()) {
            mPinnedTabsModelList.removeRange(
                    newPinnedTabs.size(), mPinnedTabsModelList.size() - newPinnedTabs.size());
        }

        mStripPropertyModel.set(
                PinnedTabStripProperties.SCROLL_TO_POSITION, mPinnedTabsModelList.size() - 1);
    }

    /**
     * Manages the visibility and animations of the pinned tabs strip. Shows the strip with an
     * animation if it's not empty, and hides it otherwise.
     */
    private void updateStripVisibility() {
        boolean shouldBeVisible = !mPinnedTabsModelList.isEmpty();
        mStripPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, shouldBeVisible);
    }

    void onSizeChanged(Size cardSize) {
        int delta =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.pinned_tab_strip_item_width_delta);
        mPinnedTabStripItemWidth = cardSize.getWidth() - delta;

        if (!mPinnedTabsModelList.isEmpty()) {
            Size newSize = new Size(mPinnedTabStripItemWidth, -1);
            for (ListItem item : mPinnedTabsModelList) {
                item.model.set(GRID_CARD_SIZE, newSize);
            }
        }
    }
}
