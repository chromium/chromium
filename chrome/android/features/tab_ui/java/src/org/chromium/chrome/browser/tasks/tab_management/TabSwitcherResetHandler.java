// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;

import java.util.List;

/** Interface to delegate resetting the tab grid. */
interface TabSwitcherResetHandler {
    /**
     * Reset the tab grid with the given {@link TabList}, which can be null.
     *
     * @param tabList The {@link TabList} to show the tabs for in the grid.
     * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
     * @return Whether the {@link TabListRecyclerView} can be shown quickly.
     */
    boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode);

    /**
     * Reset the tab grid with the given {@link List<Tab>}, which can be null.
     *
     * @param tabs The {@link List<Tab>} to show the tabs for in the grid.
     * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
     * @return Whether the {@link TabListRecyclerView} can be shown quickly.
     * @deprecated Use resetWithTabList instead to minimize the surface area of PseudoTab which
     *     should be removed with instant start. See https://crbug.com/1413207.
     */
    boolean resetWithTabs(@Nullable List<Tab> tabs, boolean quickMode);

    /**
     * Release the thumbnail {@link Bitmap} but keep the {@link TabGridView}.
     *
     * @deprecated Remove once Hub launches as this will be unused. See https://crbug.com/1516738.
     */
    void softCleanup();

    /**
     * Hard cleanup and reset the full tab list to null. Also check to see if there are any not
     * viewed price drops when the user leaves the tab switcher. This is done only before the
     * coordinator is destroyed to reduce the amount of calls to ShoppingPersistedTabData.
     *
     * @deprecated Remove once Hub launches as this will be unused. See https://crbug.com/1516738.
     */
    void hardCleanup();
}
