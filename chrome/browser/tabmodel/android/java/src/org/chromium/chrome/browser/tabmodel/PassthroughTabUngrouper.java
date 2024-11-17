// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
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
public class PassthroughTabUngrouper implements TabUngrouper {
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    /**
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public PassthroughTabUngrouper(
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    @Override
    public void ungroupTabs(
            @NonNull List<Tab> tabs,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        ungroupTabsInternal((unused) -> tabs, trailing, listener);
    }

    @Override
    public void ungroupTabGroup(
            int rootId,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        Function<TabGroupModelFilter, List<Tab>> tabsFetcher =
                (filter) -> getTabsToUngroup(filter, rootId);

        ungroupTabsInternal(tabsFetcher, trailing, listener);
    }

    @Override
    public void ungroupTabGroup(
            @NonNull Token tabGroupId,
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
        assert mTabGroupModelFilterSupplier.hasValue();
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();

        @Nullable List<Tab> tabs = tabsFetcher.apply(filter);
        if (tabs == null || tabs.isEmpty()) return;

        doUngroupTabs(filter, tabs, trailing);
        if (listener != null) {
            listener.onConfirmationDialogResult(
                    DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    static void doUngroupTabs(
            @NonNull TabGroupModelFilter filter, @NonNull List<Tab> tabs, boolean trailing) {
        for (Tab tab : tabs) {
            filter.moveTabOutOfGroupInDirection(tab.getId(), trailing);
        }
    }

    static @Nullable List<Tab> getTabsToUngroup(
            @NonNull TabGroupModelFilter filter, @NonNull Token token) {
        return getTabsToUngroup(filter, filter.getRootIdFromStableId(token));
    }

    static @Nullable List<Tab> getTabsToUngroup(@NonNull TabGroupModelFilter filter, int rootId) {
        return filter.getRelatedTabListForRootId(rootId);
    }
}
