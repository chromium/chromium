// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Passthrough implementation of the {@link TabUngrouper} interface that forwards calls directly
 * into {@link TabModel}.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public class PassthroughTabUngrouper implements TabUngrouper {
    private final Supplier<@Nullable TabModel> mTabModelSupplier;

    /**
     * @param tabModelSupplier The supplier of the {@link TabModel}.
     */
    public PassthroughTabUngrouper(Supplier<@Nullable TabModel> tabModelSupplier) {
        mTabModelSupplier = tabModelSupplier;
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
        Function<TabModel, List<Tab>> tabsFetcher =
                (tabModel) -> getTabsToUngroup(tabModel, tabGroupId);

        ungroupTabsInternal(tabsFetcher, trailing, listener);
    }

    private void ungroupTabsInternal(
            Function<TabModel, List<Tab>> tabsFetcher,
            boolean trailing,
            @Nullable TabModelActionListener listener) {
        TabModelInternal tabModel = getTabModelInternal();
        @Nullable List<Tab> tabs = tabsFetcher.apply(tabModel);
        if (tabs == null || tabs.isEmpty()) return;

        if (listener != null) {
            listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        }
        doUngroupTabs(tabModel, tabs, trailing);
        if (listener != null) {
            listener.onConfirmationDialogResult(
                    DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    static void doUngroupTabs(TabModelInternal tabModel, List<Tab> tabs, boolean trailing) {
        for (Tab tab : tabs) {
            tabModel.moveTabOutOfGroupInDirection(tab.getId(), trailing);
        }
    }

    static List<Tab> getTabsToUngroup(TabModel tabModel, Token token) {
        return tabModel.getTabsInGroup(token);
    }

    private TabModelInternal getTabModelInternal() {
        TabModelInternal tabModel = (TabModelInternal) mTabModelSupplier.get();
        assert tabModel != null;
        return tabModel;
    }
}
