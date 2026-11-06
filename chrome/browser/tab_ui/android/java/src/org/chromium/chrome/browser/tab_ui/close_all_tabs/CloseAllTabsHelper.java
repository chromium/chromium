// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui.close_all_tabs;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.List;
import java.util.function.Supplier;

/** Helper for closing all tabs via {@link CloseAllTabsDialog}. */
@NullMarked
public class CloseAllTabsHelper {
    private CloseAllTabsHelper() {}

    /**
     * Closes all tabs hiding tab groups with an explicit closing source.
     *
     * @param tabModelSelector {@link TabModelSelector} for the activity.
     * @param unarchiveAndGetUndoRunnable Synchronously mutates the regular {@link TabModel} by
     *     unarchiving archived tabs so they can be closed alongside regular tabs, and returns an
     *     undo runnable. Must execute even when {@code allowUndo} is {@code false}.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     * @param closingSource The closing source of the tab closure.
     * @param closePinnedTabs Whether pinned tabs should also be closed when the feature flag is
     *     enabled.
     */
    public static void closeAllTabsHidingTabGroups(
            TabModelSelector tabModelSelector,
            Supplier<Runnable> unarchiveAndGetUndoRunnable,
            boolean allowUndo,
            @TabClosingSource int closingSource,
            boolean closePinnedTabs) {
        ThreadUtils.assertOnUiThread();
        closeAllIncognitoTabs(
                tabModelSelector, allowUndo, /* hideTabGroups= */ true, closingSource);

        TabModel regularTabModel = tabModelSelector.getModel(/* incognito= */ false);
        if (regularTabModel.getTabModelType() == TabModelType.EMPTY) {
            return;
        }
        // Unarchive archived tabs first so retainPinnedTabs and unpinned-tab collection inspect the
        // regular TabModel after all unarchived tabs are restored into it.
        Runnable undoRunnable = unarchiveAndGetUndoRunnable.get();

        boolean retainPinnedTabs =
                !closePinnedTabs
                        && ChromeFeatureList.sAndroidCloseAllTabsRetainsPinnedTabs.isEnabled()
                        && regularTabModel.getPinnedTabsCount() > 0;

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

        if (allowUndo) {
            paramsBuilder.withUndoRunnable(undoRunnable);
        }
        regularTabModel
                .getTabRemover()
                .closeTabs(
                        paramsBuilder
                                .hideTabGroups(true)
                                .allowUndo(allowUndo)
                                .tabClosingSource(closingSource)
                                .build(),
                        /* allowDialog= */ false);
    }

    /**
     * Creates a callback to close all tabs, accepting a boolean indicating whether pinned tabs
     * should also be closed.
     *
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param unarchiveAndGetUndoRunnable Synchronously mutates the regular {@link TabModel} by
     *     unarchiving archived tabs so they can be closed alongside regular tabs, and returns an
     *     undo runnable. Must execute even when {@code allowUndo} is {@code false}.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     */
    public static Callback<Boolean> buildCloseAllTabsCallback(
            TabModelSelector tabModelSelector,
            Supplier<Runnable> unarchiveAndGetUndoRunnable,
            boolean allowUndo) {
        ThreadUtils.assertOnUiThread();
        return (closePinnedTabs) ->
                closeAllTabsHidingTabGroups(
                        tabModelSelector,
                        unarchiveAndGetUndoRunnable,
                        allowUndo,
                        TabClosingSource.GRID_TAB_SWITCHER,
                        closePinnedTabs);
    }

    /**
     * Creates a callback to close all incognito tabs; the {@link Boolean} callback parameter is
     * ignored.
     *
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param allowUndo See {@link TabClosureParams#allowUndo}.
     */
    public static Callback<Boolean> buildCloseAllIncognitoTabsCallback(
            TabModelSelector tabModelSelector, boolean allowUndo) {
        ThreadUtils.assertOnUiThread();
        return (_) ->
                closeAllIncognitoTabs(
                        tabModelSelector,
                        allowUndo,
                        /* hideTabGroups= */ false,
                        TabClosingSource.GRID_TAB_SWITCHER);
    }

    private static void closeAllIncognitoTabs(
            TabModelSelector tabModelSelector,
            boolean allowUndo,
            boolean hideTabGroups,
            @TabClosingSource int closingSource) {
        ThreadUtils.assertOnUiThread();
        tabModelSelector
                .getModel(/* incognito= */ true)
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .allowUndo(allowUndo)
                                .hideTabGroups(hideTabGroups)
                                .tabClosingSource(closingSource)
                                .build(),
                        /* allowDialog= */ false);
    }
}
