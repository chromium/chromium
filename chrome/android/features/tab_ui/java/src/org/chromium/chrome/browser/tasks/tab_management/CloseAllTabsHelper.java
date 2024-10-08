// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import java.util.ArrayList;
import java.util.List;

/** Helper for closing all tabs via {@link CloseAllTabsDialog}. */
public class CloseAllTabsHelper {
    /** Closes all tabs hiding tab groups. */
    public static void closeAllTabsHidingTabGroups(
            TabModelSelector tabModelSelector, TabCreator regularTabCreator) {
        var filterProvider = tabModelSelector.getTabModelFilterProvider();
        ((TabGroupModelFilter) filterProvider.getTabModelFilter(true))
                .closeTabs(TabClosureParams.closeAllTabs().hideTabGroups(true).build());

        Runnable undoRunnable = () -> {};
        if (ChromeFeatureList.sAndroidTabDeclutter.isEnabled()) {
            final Profile profile =
                    tabModelSelector.getCurrentModel().getProfile().getOriginalProfile();
            List<Integer> previouslyArchivedTabIds =
                    unarchiveTabsForTabClosure(profile, regularTabCreator);
            undoRunnable =
                    () ->
                            archiveTabsAfterTabClosureUndo(
                                    profile,
                                    tabModelSelector.getModel(/* incognito= */ false),
                                    previouslyArchivedTabIds);
        }
        ((TabGroupModelFilter) filterProvider.getTabModelFilter(false))
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .hideTabGroups(true)
                                .withUndoRunnable(undoRunnable)
                                .build());
    }

    /**
     * Create a runnable to close all tabs using appropriate animations where applicable.
     *
     * @param tabModelSelector The tab model selector for the activity.
     * @param isIncognitoOnly Whether to only close incognito tabs.
     */
    public static Runnable buildCloseAllTabsRunnable(
            TabModelSelector tabModelSelector,
            TabCreator regularTabCreator,
            boolean isIncognitoOnly) {
        return () -> closeAllTabs(tabModelSelector, regularTabCreator, isIncognitoOnly);
    }

    private static void closeAllTabs(
            TabModelSelector tabModelSelector,
            TabCreator regularTabCreator,
            boolean isIncognitoOnly) {
        if (isIncognitoOnly) {
            tabModelSelector
                    .getModel(/* isIncognito= */ true)
                    .closeTabs(TabClosureParams.closeAllTabs().build());
        } else {
            closeAllTabsHidingTabGroups(tabModelSelector, regularTabCreator);
        }
    }

    private static List<Integer> unarchiveTabsForTabClosure(
            Profile profile, TabCreator regularTabCreator) {
        List<Integer> previouslyArchivedTabIds = new ArrayList<>();
        ArchivedTabModelOrchestrator orchestrator =
                ArchivedTabModelOrchestrator.getForProfile(profile);
        TabArchiver archiver = orchestrator.getTabArchiver();
        TabModel archivedTabModel = orchestrator.getTabModel();
        while (archivedTabModel.getCount() > 0) {
            Tab archivedTab = archivedTabModel.getTabAt(0);
            previouslyArchivedTabIds.add(archivedTab.getId());
            archiver.unarchiveAndRestoreTab(regularTabCreator, archivedTab);
        }
        return previouslyArchivedTabIds;
    }

    private static void archiveTabsAfterTabClosureUndo(
            Profile profile, TabModel regularTabModel, List<Integer> previouslyArchivedTabIds) {
        ArchivedTabModelOrchestrator orchestrator =
                ArchivedTabModelOrchestrator.getForProfile(profile);
        TabArchiver archiver = orchestrator.getTabArchiver();
        for (int i = 0; i < regularTabModel.getCount(); ) {
            Tab tab = regularTabModel.getTabAt(i);
            if (previouslyArchivedTabIds.contains(tab.getId())) {
                archiver.archiveAndRemoveTab(regularTabModel, tab);
            } else {
                i++;
            }
        }
    }
}
