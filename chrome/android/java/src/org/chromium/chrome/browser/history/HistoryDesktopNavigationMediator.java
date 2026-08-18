// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.navigation_pane.NavigationPaneProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the history desktop navigation pane. */
@NullMarked
class HistoryDesktopNavigationMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private final Runnable mOnHistoryClicked;
    private final Runnable mOnTabsFromOtherDevicesClicked;
    private PropertyModel mHistoryModel;
    private PropertyModel mTabsFromOtherDevicesModel;

    /**
     * Constructs a {@link HistoryDesktopNavigationMediator}.
     *
     * @param context The current Activity context.
     * @param modelList The ModelList to populate.
     * @param onHistoryClicked Runnable executed when the history item is clicked.
     * @param onTabsFromOtherDevicesClicked Runnable executed when the tabs from other devices item
     *     is clicked.
     */
    public HistoryDesktopNavigationMediator(
            Context context,
            ModelList modelList,
            Runnable onHistoryClicked,
            Runnable onTabsFromOtherDevicesClicked) {
        mContext = context;
        mModelList = modelList;
        mOnHistoryClicked = onHistoryClicked;
        mOnTabsFromOtherDevicesClicked = onTabsFromOtherDevicesClicked;
        populateModelList();
    }

    private void populateModelList() {
        mModelList.clear();

        mHistoryModel =
                new PropertyModel.Builder(NavigationPaneProperties.NAVIGATION_ITEM_KEYS)
                        .with(
                                NavigationPaneProperties.TITLE,
                                mContext.getString(R.string.chrome_history))
                        .with(
                                NavigationPaneProperties.ICON,
                                AppCompatResources.getDrawable(
                                        mContext, R.drawable.ic_history_24dp))
                        .with(NavigationPaneProperties.IS_SELECTED, true)
                        .with(NavigationPaneProperties.ON_CLICK_HANDLER, this::onHistoryClicked)
                        .build();
        mModelList.add(
                new ListItem(NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM, mHistoryModel));

        mTabsFromOtherDevicesModel =
                new PropertyModel.Builder(NavigationPaneProperties.NAVIGATION_ITEM_KEYS)
                        .with(
                                NavigationPaneProperties.TITLE,
                                mContext.getString(
                                        R.string.history_manager_tabs_from_other_devices))
                        .with(
                                NavigationPaneProperties.ICON,
                                AppCompatResources.getDrawable(
                                        mContext, R.drawable.devices_black_24dp))
                        .with(NavigationPaneProperties.IS_SELECTED, false)
                        .with(
                                NavigationPaneProperties.ON_CLICK_HANDLER,
                                this::onTabsFromOtherDevicesClicked)
                        .build();
        mModelList.add(
                new ListItem(
                        NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM,
                        mTabsFromOtherDevicesModel));
    }

    private void onHistoryClicked() {
        mHistoryModel.set(NavigationPaneProperties.IS_SELECTED, true);
        mTabsFromOtherDevicesModel.set(NavigationPaneProperties.IS_SELECTED, false);
        mOnHistoryClicked.run();
    }

    private void onTabsFromOtherDevicesClicked() {
        mHistoryModel.set(NavigationPaneProperties.IS_SELECTED, false);
        mTabsFromOtherDevicesModel.set(NavigationPaneProperties.IS_SELECTED, true);
        mOnTabsFromOtherDevicesClicked.run();
    }

    /** Destroys the mediator. */
    public void destroy() {}
}
