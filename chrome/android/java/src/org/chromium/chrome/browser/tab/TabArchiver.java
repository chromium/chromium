// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

/** Responsible for moving tabs to/from the archived {@link TabModel}. */
public class TabArchiver {
    private final TabCreator mArchivedTabCreator;
    private final TabModel mArchivedTabModel;
    private final AsyncTabParamsManager mAsyncTabParamsManager;

    /**
     * @param archivedTabCreator The {@link TabCreator} for the archived TabModel.
     * @param archivedTabModel The {@link TabModel} for archived tabs.
     * @param asyncTabParamsManager The {@link AsyncTabParamsManager} used when unarchiving tabs.
     */
    public TabArchiver(
            TabCreator archivedTabCreator,
            TabModel archivedTabModel,
            AsyncTabParamsManager asyncTabParamsManager) {
        mArchivedTabCreator = archivedTabCreator;
        mArchivedTabModel = archivedTabModel;
        mAsyncTabParamsManager = asyncTabParamsManager;
    }

    /**
     * Create an archived copy of the given Tab in the archived TabModel, and close the Tab in the
     * regular TabModel. Must be called on the UI thread.
     *
     * @param tabModel The {@link TabModel} the tab currently belongs to.
     * @param tab The {@link Tab} to unarchive.
     */
    public void archiveAndRemoveTab(TabModel tabModel, Tab tab) {
        ThreadUtils.assertOnUiThread();
        TabState tabState = TabStateExtractor.from(tab);
        mArchivedTabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
        tabModel.closeTab(tab);
    }

    /**
     * Unarchive the given tab, moving it into the normal TabModel. The tab is reused between the
     * archived/regular TabModels. Must be called on the UI thread.
     *
     * @param tabCreator The {@link TabCreator} to use when recreating the tabs.
     * @param tab The {@link Tab} to unarchive.
     */
    public void unarchiveAndRestoreTab(TabCreator tabCreator, Tab tab) {
        ThreadUtils.assertOnUiThread();
        TabState tabState = TabStateExtractor.from(tab);
        mArchivedTabModel.removeTab(tab);
        mAsyncTabParamsManager.add(tab.getId(), new TabReparentingParams(tab, null));
        tabCreator.createFrozenTab(tabState, tab.getId(), INVALID_TAB_INDEX);
    }
}
