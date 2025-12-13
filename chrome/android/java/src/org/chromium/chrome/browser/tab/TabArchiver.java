// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.List;

/** Responsible for moving tabs to/from the archived {@link TabModel}. */
@NullMarked
public interface TabArchiver extends Destroyable {

    /** Provides an interface to observer the declutter process. */
    interface Observer {
        /** Called when a declutter pass is completed. */
        default void onDeclutterPassCompleted() {}

        /** Called when the persisted tab data for the archive pass is created. */
        default void onArchivePersistedTabDataCreated() {}

        /** Called when an autodelete pass is completed. */
        default void onAutodeletePassCompleted() {}
    }

    /** Adds an observer to the class. */
    void addObserver(Observer observer);

    /** Removes an observer from the class. */
    void removeObserver(Observer observer);

    /**
     * Do an archive pass of the main {@link TabModelSelector}.
     *
     * <p>1. Iterates through all known tab model selects, and archives inactive tabs. 2. Iterates
     * through all archived tabs, and automatically deletes those old enough (only if auto-deletion
     * is enabled through settings).
     *
     * @param selectorToArchive The {@link TabModelSelector} to archive.
     */
    void doArchivePass(TabModelSelector selectorToArchive);

    /** Delete eligible archived tabs. */
    void doAutodeletePass();

    /**
     * Create an archived copy of the given Tab in the archived TabModel, and close the Tab in the
     * regular TabModel. Must be called on the UI thread.
     *
     * @param regularTabGroupModelFilter The {@link TabGroupModelFilter} the tab currently belongs
     *     to.
     * @param tabs The list {@link Tab}s to unarchive.
     */
    void archiveAndRemoveTabs(TabGroupModelFilter regularTabGroupModelFilter, List<Tab> tabs);

    /**
     * Unarchive the given tab, moving it into the normal TabModel. The tab is reused between the
     * archived/regular TabModels. Must be called on the UI thread.
     *
     * @param tabCreator The {@link TabCreator} to use when recreating the tabs.
     * @param tabs The {@link Tab}s to unarchive.
     * @param updateTimestamp Whether the Tab's timestamp should be updated.
     * @param areTabsBeingOpened Whether the restored tab is being opened.
     */
    void unarchiveAndRestoreTabs(
            TabCreator tabCreator,
            List<Tab> tabs,
            boolean updateTimestamp,
            boolean areTabsBeingOpened);

    /**
     * Rescue all archived tabs, and store them in the given {@link TabCreator}.
     *
     * @param regularTabCreator The {@link TabCreator} for the current window's regular {@link
     *     TabModel}.
     */
    void rescueArchivedTabs(TabCreator regularTabCreator);
}
