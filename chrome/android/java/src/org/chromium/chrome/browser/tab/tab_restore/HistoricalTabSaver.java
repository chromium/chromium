// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;
import java.util.function.Supplier;

/** Interface for creating entries in TabRestoreService. */
@NullMarked
public interface HistoricalTabSaver {
    /**
     * Returned when no TabRestoreService entry was stored. Equal to
     * sessions::SessionID::InvalidValue(). Valid ids are > 0.
     */
    @TabRestoreEntryId int INVALID_TAB_RESTORE_ENTRY_ID = -1;

    /** Destroys the instance. */
    void destroy();

    /**
     * Adds a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    void addSecondaryTabModelSupplier(Supplier<@Nullable TabModel> tabModelSupplier);

    /**
     * Removes a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    void removeSecondaryTabModelSupplier(Supplier<@Nullable TabModel> tabModelSupplier);

    /**
     * Creates a Tab entry in TabRestoreService.
     *
     * @param tab The {@link Tab} to create an entry for.
     * @return the id of the stored Tab entry, or INVALID_TAB_RESTORE_ENTRY_ID.
     */
    @TabRestoreEntryId
    int createHistoricalTab(Tab tab);

    /**
     * Creates a Group or Tab entry in TabRestoreService.
     *
     * @param entry The {@link HistoricalEntry} to use for entry creation.
     * @return the id of the stored top-level entry, or INVALID_TAB_RESTORE_ENTRY_ID.
     */
    @TabRestoreEntryId
    int createHistoricalTabOrGroup(HistoricalEntry entry);

    /**
     * Creates a Window entry in TabRestoreService. This corresponds to a bulk closure which is
     * defined as when any of the following are closed simultaneously; - Two or more ungrouped tabs.
     * - Two or more groups of tabs. - At least one group and one tab.
     *
     * @param entries An in-order list of {@link HistoricalEntry}s to create a single
     *     TabRestoreService entry for.
     * @return the id of the single top-level entry stored for these entries: a Tab or Group entry
     *     if validation leaves exactly one of those, otherwise a Window entry (which may end up
     *     holding a single tab); INVALID_TAB_RESTORE_ENTRY_ID if nothing was stored.
     */
    @TabRestoreEntryId
    int createHistoricalBulkClosure(List<HistoricalEntry> entries);
}
