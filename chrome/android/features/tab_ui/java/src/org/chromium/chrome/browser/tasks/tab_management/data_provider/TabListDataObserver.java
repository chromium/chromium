// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Observer interface for data change events emitted by {@link TabListDataProvider}. */
@NullMarked
public interface TabListDataObserver {
    /**
     * Called when the entire data set is reset.
     *
     * @param items The new full list of {@link TabListItem} entries.
     */
    default void onDataReset(List<? extends TabListItem> items) {
        // TODO(crbug.com/562590772): Triggered by TabListMediator#resetWithListOfTabs (via
        // TabListDataProvider#requestDataReset) and TabModelObserver#restoreCompleted.
        // TabListMediator will update existing PropertyModels in place via updateTab() when the
        // item sequence is unchanged, or rebuild tab entries in TabListModel via
        // TabListMediator#addTabInfoToModel() while preserving non-tab items.
    }

    /**
     * Called when items are inserted into the data set.
     *
     * @param items The inserted {@link TabListItem} entries.
     * @param after The item immediately preceding the insertion, or null if inserted at the start
     *     of the list.
     */
    default void onItemsInserted(List<? extends TabListItem> items, @Nullable TabListItem after) {
        // TODO(crbug.com/562590772): TabListMediator will listen to this callback on
        // TabListDataObserver instead of observing TabModelObserver#didAddTab directly, resolving
        // `after` via TabListModel#indexFromTabId (or the start of the tab region when `after` is
        // null) and inserting each TabItem via TabListMediator#addTabInfoToModel().
    }

    /**
     * Called when items are removed from the data set.
     *
     * @param items The removed {@link TabListItem} entries.
     */
    default void onItemsRemoved(List<? extends TabListItem> items) {
        // TODO(crbug.com/562590772): TabListMediator will listen to this callback on
        // TabListDataObserver instead of observing TabModelObserver#didRemoveTabForClosure /
        // TabModelObserver#tabRemoved directly, resolving each item via TabListModel#indexFromTabId
        // and removing it from TabListModel.
    }

    /**
     * Called when items are moved within the data set.
     *
     * @param items The moved {@link TabListItem} entries.
     * @param after The item immediately preceding the new position, or null if moved to the start
     *     of the list.
     */
    default void onItemsMoved(List<? extends TabListItem> items, @Nullable TabListItem after) {
        // TODO(crbug.com/562590772): TabListMediator will listen to this callback on
        // TabListDataObserver instead of observing TabModelObserver#didMoveTab directly, resolving
        // `after` via TabListModel#indexFromTabId (or the start of the tab region when `after` is
        // null) and moving each TabItem in TabListModel.
    }

    // TODO(crbug.com/562590772): Add remaining structural and property update callbacks.
}
