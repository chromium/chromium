// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

/** Helper for closing all tabs via {@link CloseAllTabsDialog}. */
public class CloseAllTabsHelper {
    /** Closes all tabs hiding tab groups. */
    public static void closeAllTabsHidingTabGroups(TabModelSelector tabModelSelector) {
        var filterProvider = tabModelSelector.getTabModelFilterProvider();
        TabClosureParams params = TabClosureParams.closeAllTabs().hideTabGroups(true).build();
        ((TabGroupModelFilter) filterProvider.getTabModelFilter(false)).closeTabs(params);
        ((TabGroupModelFilter) filterProvider.getTabModelFilter(true)).closeTabs(params);
    }

    /**
     * Create a runnable to close all tabs using appropriate animations where applicable.
     *
     * @param tabModelSelector The tab model selector for the activity.
     * @param isIncognitoOnly Whether to only close incognito tabs.
     */
    public static Runnable buildCloseAllTabsRunnable(
            TabModelSelector tabModelSelector, boolean isIncognitoOnly) {
        return () -> closeAllTabs(tabModelSelector, isIncognitoOnly);
    }

    private static void closeAllTabs(TabModelSelector tabModelSelector, boolean isIncognitoOnly) {
        if (isIncognitoOnly) {
            tabModelSelector
                    .getModel(/* isIncognito= */ true)
                    .closeTabs(TabClosureParams.closeAllTabs().build());
        } else {
            closeAllTabsHidingTabGroups(tabModelSelector);
        }
    }
}
