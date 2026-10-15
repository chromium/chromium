// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
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
     * @param closePinnedTabs Whether pinned tabs should also be closed when the feature flag is
     *     enabled.
     */
    public static void closeAllTabsHidingTabGroups(
            TabModelSelector tabModelSelector, boolean allowUndo, boolean closePinnedTabs) {
        closeAllTabsHidingTabGroups(
                tabModelSelector, allowUndo, TabClosingSource.GRID_TAB_SWITCHER, closePinnedTabs);
    }

    /**
     * Closes all tabs hiding tab groups with an explicit closing source.
     *
     * @param tabModelSelector {@link TabModelSelector} for the activity.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     * @param closingSource The closing source of the tab closure.
     * @param closePinnedTabs Whether pinned tabs should also be closed when the feature flag is
     *     enabled.
     */
    public static void closeAllTabsHidingTabGroups(
            TabModelSelector tabModelSelector,
            boolean allowUndo,
            @TabClosingSource int closingSource,
            boolean closePinnedTabs) {
        tabModelSelector
                .getModel(/* incognito= */ true)
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .allowUndo(allowUndo)
                                .hideTabGroups(true)
                                .tabClosingSource(closingSource)
                                .build(),
                        /* allowDialog= */ false);

        TabModel regularTabModel = tabModelSelector.getModel(/* incognito= */ false);
        boolean retainPinnedTabs =
                !closePinnedTabs
                        && ChromeFeatureList.sAndroidCloseAllTabsRetainsPinnedTabs.isEnabled()
                        && regularTabModel.getPinnedTabsCount() > 0;

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

        TabClosureParams.Builder paramsBuilder;
        if (retainPinnedTabs) {
            List<Tab> unpinnedTabs = TabModelUtils.getUnpinnedTabs(regularTabModel);
            if (unpinnedTabs.isEmpty()) {
                return;
            }
            paramsBuilder = TabClosureParams.closeTabs(unpinnedTabs);
        } else {
            paramsBuilder = TabClosureParams.closeAllTabs();
        }

        regularTabModel
                .getTabRemover()
                .closeTabs(
                        paramsBuilder
                                .hideTabGroups(true)
                                .allowUndo(allowUndo)
                                .withUndoRunnable(restoreArchivedTabsRunnable)
                                .tabClosingSource(closingSource)
                                .build(),
                        /* allowDialog= */ false);
    }

    /**
     * Create a callback to close all tabs, accepting a boolean indicating whether pinned tabs
     * should also be closed.
     *
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param isIncognitoOnly Whether to only close incognito tabs.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     */
    public static Callback<Boolean> buildCloseAllTabsCallback(
            TabModelSelector tabModelSelector, boolean isIncognitoOnly, boolean allowUndo) {
        return (closePinnedTabs) ->
                closeAllTabs(tabModelSelector, isIncognitoOnly, allowUndo, closePinnedTabs);
    }

    private static void closeAllTabs(
            TabModelSelector tabModelSelector,
            boolean isIncognitoOnly,
            boolean allowUndo,
            boolean closePinnedTabs) {
        if (isIncognitoOnly) {
            tabModelSelector
                    .getModel(/* incognito= */ true)
                    .getTabRemover()
                    .closeTabs(
                            TabClosureParams.closeAllTabs()
                                    .allowUndo(allowUndo)
                                    .tabClosingSource(TabClosingSource.GRID_TAB_SWITCHER)
                                    .build(),
                            /* allowDialog= */ false);
        } else {
            closeAllTabsHidingTabGroups(
                    tabModelSelector,
                    allowUndo,
                    TabClosingSource.GRID_TAB_SWITCHER,
                    closePinnedTabs);
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
            TabModel tabModel = tabModelSelector.getModel(/* incognito= */ false);
            archiveTabsAfterTabClosureUndo(
                    archivedOrchestrator, tabModel, previouslyArchivedTabIds);
        };
    }

    private static List<Integer> unarchiveTabsForTabClosure(
            ArchivedTabModelOrchestrator archivedOrchestrator, TabCreator regularTabCreator) {
        assert archivedOrchestrator.areTabModelsInitialized();

        TabArchiver archiver = archivedOrchestrator.getTabArchiver();
        TabModel archivedTabModel = archivedOrchestrator.getTabModel();
        assumeNonNull(archivedTabModel);
        List<Integer> previouslyArchivedTabIds = new ArrayList<>(archivedTabModel.getCount());
        for (Tab archivedTab : archivedTabModel) {
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
            TabModel regularTabModel,
            List<Integer> previouslyArchivedTabIds) {
        assert archivedOrchestrator.areTabModelsInitialized();

        TabArchiver archiver = archivedOrchestrator.getTabArchiver();
        List<Tab> tabsToArchive = new ArrayList<>();
        for (int i = 0; i < regularTabModel.getCount(); i++) {
            Tab tab = regularTabModel.getTabAtChecked(i);
            if (previouslyArchivedTabIds.contains(tab.getId())) {
                tabsToArchive.add(tab);
            }
        }

        archiver.archiveAndRemoveTabs(regularTabModel, tabsToArchive);
    }
}
