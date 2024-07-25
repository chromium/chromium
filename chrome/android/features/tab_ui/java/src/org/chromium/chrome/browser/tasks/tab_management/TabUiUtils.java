// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;

import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/** Static utilities for Tab UI. */
public class TabUiUtils {

    /**
     * Closes a tab group and maybe shows a confirmation dialog.
     *
     * @param filter The {@link TabGroupModelFilter} to act on.
     * @param actionConfirmationManager The {@link ActionConfirmationManager} to use to confirm
     *     actions.
     * @param tabId The ID of one of the tabs in the tab group.
     * @param hideTabGroups Whether to hide or delete the tab group.
     */
    public static void closeTabGroup(
            TabGroupModelFilter filter,
            ActionConfirmationManager actionConfirmationManager,
            int tabId,
            boolean hideTabGroups) {
        TabModel tabModel = filter.getTabModel();
        int rootId = tabModel.getTabById(tabId).getRootId();
        List<Tab> tabs = filter.getRelatedTabListForRootId(rootId);
        boolean isIncognito = filter.isIncognitoBranded();

        if (hideTabGroups || isIncognito) {
            filter.closeMultipleTabs(tabs, /* canUndo= */ true, hideTabGroups);
        } else {
            List<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toList());

            // Present a confirmation dialog to the user before closing the tab group.
            Callback<Integer> onResult =
                    (@ConfirmationResult Integer result) -> {
                        if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                            boolean canUndo = result == ConfirmationResult.IMMEDIATE_CONTINUE;
                            List<Tab> tabsToClose =
                                    tabIds.stream()
                                            .map(filter.getTabModel()::getTabById)
                                            .filter(Objects::nonNull)
                                            .filter(tab -> !tab.isClosing())
                                            .collect(Collectors.toList());
                            filter.closeMultipleTabs(tabsToClose, canUndo, hideTabGroups);
                        }
                    };
            actionConfirmationManager.processDeleteGroupAttempt(onResult);
        }
    }
}
