// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.List;

/** Helper for closing all tabs via {@link CloseAllTabsDialog}. */
@NullMarked
public class CloseAllTabsHelper {
    /**
     * Closes all tabs hiding tab groups.
     *
     * @param tabModelSelector {@link TabModelSelector} for the activity.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     */
    public static void closeAllTabsHidingTabGroups(
            TabModelSelector tabModelSelector, boolean allowUndo) {
        tabModelSelector
                .getModel(/* incognito= */ true)
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .allowUndo(allowUndo)
                                .hideTabGroups(true)
                                .build(),
                        /* allowDialog= */ false);

        // To support CloseAllTabs for archived tabs, the tabs are restored/deleted but remain
        // cached. If a user undos the operation, then those tabs are re-archived. It's possible
        // that the archived infrastructure isn't fully initialized at this point. In that case,
        // archived tabs will be skipped entirely.
        final ArchivedTabModelOrchestrator archivedOrchestrator =
                ArchivedTabModelOrchestrator.getForProfile(
                        assumeNonNull(tabModelSelector.getCurrentModel().getProfile())
                                .getOriginalProfile());
        Runnable restoreArchivedTabsRunnable =
                removeArchivedTabsAndGetUndoRunnable(archivedOrchestrator, tabModelSelector);

        tabModelSelector
                .getModel(/* incognito= */ false)
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .allowUndo(allowUndo)
                                .hideTabGroups(true)
                                .withUndoRunnable(restoreArchivedTabsRunnable)
                                .build(),
                        /* allowDialog= */ false);
    }

    /**
     * Create a runnable to close all tabs using appropriate animations where applicable.
     *
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param isIncognitoOnly Whether to only close incognito tabs.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     */
    public static Runnable buildCloseAllTabsRunnable(
            TabModelSelector tabModelSelector, boolean isIncognitoOnly, boolean allowUndo) {
        return () -> closeAllTabs(tabModelSelector, isIncognitoOnly, allowUndo);
    }

    private static void closeAllTabs(
            TabModelSelector tabModelSelector, boolean isIncognitoOnly, boolean allowUndo) {
        if (isIncognitoOnly) {
            tabModelSelector
                    .getModel(/* incognito= */ true)
                    .getTabRemover()
                    .closeTabs(
                            TabClosureParams.closeAllTabs().allowUndo(allowUndo).build(),
                            /* allowDialog= */ false);
        } else {
            closeAllTabsHidingTabGroups(tabModelSelector, allowUndo);
        }
    }

    @VisibleForTesting
    static Runnable removeArchivedTabsAndGetUndoRunnable(
            ArchivedTabModelOrchestrator archivedOrchestrator, TabModelSelector tabModelSelector) {
        if (!archivedOrchestrator.areTabModelsInitialized()) {
            return CallbackUtils.emptyRunnable();
        }
        List<Integer> previouslyArchivedTabIds =
                unarchiveTabsForTabClosure(
                        archivedOrchestrator,
                        tabModelSelector
                                .getTabCreatorManager()
                                .getTabCreator(/* incognito= */ false));
        return () -> {
            TabGroupModelFilter filter =
                    assumeNonNull(
                            tabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(/* isIncognito= */ false));
            archiveTabsAfterTabClosureUndo(archivedOrchestrator, filter, previouslyArchivedTabIds);
        };
    }

    private static List<Integer> unarchiveTabsForTabClosure(
            ArchivedTabModelOrchestrator archivedOrchestrator, TabCreator regularTabCreator) {
        assert archivedOrchestrator.areTabModelsInitialized();
        List<Integer> previouslyArchivedTabIds = new ArrayList<>();

        TabArchiver archiver = archivedOrchestrator.getTabArchiver();
        TabModel archivedTabModel = archivedOrchestrator.getTabModel();
        for (int i = 0; i < archivedTabModel.getCount(); i++) {
            Tab archivedTab = archivedTabModel.getTabAtChecked(i);
            previouslyArchivedTabIds.add(archivedTab.getId());
        }
        archiver.unarchiveAndRestoreTabs(
                regularTabCreator,
                TabModelUtils.convertTabListToListOfTabs(archivedTabModel),
                /* updateTimestamp= */ true,
                /* areTabsBeingOpened= */ false);
        return previouslyArchivedTabIds;
    }

    private static void archiveTabsAfterTabClosureUndo(
            ArchivedTabModelOrchestrator archivedOrchestrator,
            TabGroupModelFilter regularTabGroupModelFilter,
            List<Integer> previouslyArchivedTabIds) {
        assert archivedOrchestrator.areTabModelsInitialized();

        TabModel regularTabModel = regularTabGroupModelFilter.getTabModel();
        TabArchiver archiver = archivedOrchestrator.getTabArchiver();
        List<Tab> tabsToArchive = new ArrayList<>();
        for (int i = 0; i < regularTabModel.getCount(); i++) {
            Tab tab = regularTabModel.getTabAtChecked(i);
            if (previouslyArchivedTabIds.contains(tab.getId())) {
                tabsToArchive.add(tab);
            }
        }

        archiver.archiveAndRemoveTabs(regularTabGroupModelFilter, tabsToArchive);
    }
}
