// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.util.List;
import java.util.function.Function;

/**
 * Passthrough implementation of the {@link TabUngrouper} interface that forwards calls directly
 * into {@link TabGroupModelFilter}.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public class PassthroughTabUngrouper implements TabUngrouper {
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    /**
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public PassthroughTabUngrouper(Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    @Override
    public void ungroupTabs(
            List<Tab> tabs,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        ungroupTabsInternal((unused) -> tabs, trailing, listener);
    }

    @Override
    public void ungroupTabGroup(
            Token tabGroupId,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        Function<TabGroupModelFilter, List<Tab>> tabsFetcher =
                (filter) -> getTabsToUngroup(filter, tabGroupId);

        ungroupTabsInternal(tabsFetcher, trailing, listener);
    }

    private void ungroupTabsInternal(
            Function<TabGroupModelFilter, List<Tab>> tabsFetcher,
            boolean trailing,
            @Nullable TabModelActionListener listener) {
        TabGroupModelFilterInternal filter = getTabGroupModelFilter();
        @Nullable List<Tab> tabs = tabsFetcher.apply(filter);
        if (tabs == null || tabs.isEmpty()) return;

        if (listener != null) {
            listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        }
        doUngroupTabs(filter, tabs, trailing);
        if (listener != null) {
            listener.onConfirmationDialogResult(
                    DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    static void doUngroupTabs(
            TabGroupModelFilterInternal filter, List<Tab> tabs, boolean trailing) {
        for (Tab tab : tabs) {
            filter.moveTabOutOfGroupInDirection(tab.getId(), trailing);
        }
    }

    static List<Tab> getTabsToUngroup(TabGroupModelFilter filter, Token token) {
        return filter.getTabsInGroup(token);
    }

    private TabGroupModelFilterInternal getTabGroupModelFilter() {

        @Nullable TabGroupModelFilterInternal tabGroupModelFilter =
                (TabGroupModelFilterInternal) mTabGroupModelFilterSupplier.get();
        assert tabGroupModelFilter != null;
        return tabGroupModelFilter;
    }
}
