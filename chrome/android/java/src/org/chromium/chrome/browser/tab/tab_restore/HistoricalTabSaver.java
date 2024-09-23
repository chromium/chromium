// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;

/** Interface for creating entries in TabRestoreService. */
public interface HistoricalTabSaver {
    /** Destroys the instance. */
    public void destroy();

    /**
     * Adds a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    public void addSecodaryTabModelSupplier(Supplier<TabModel> tabModelSupplier);

    /**
     * Removes a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    public void removeSecodaryTabModelSupplier(Supplier<TabModel> tabModelSupplier);

    /**
     * Creates a Tab entry in TabRestoreService.
     *
     * @param tab The {@link Tab} to create an entry for.
     */
    void createHistoricalTab(Tab tab);

    /**
     * Creates a Group or Tab entry in TabRestoreService.
     * @param entry The {@link HistoricalEntry} to use for entry creation.
     */
    void createHistoricalTabOrGroup(HistoricalEntry entry);

    /**
     * Creates a Window entry in TabRestoreService. This corresponds to a bulk closure which is
     * defined as when any of the following are closed simultaneously;
     * - Two or more ungrouped tabs.
     * - Two or more groups of tabs.
     * - At least one group and one tab.
     * @param entries An in-order list of {@link HistoricalEntry}s to create a single
     *                TabRestoreService entry for.
     */
    void createHistoricalBulkClosure(List<HistoricalEntry> entries);
}
