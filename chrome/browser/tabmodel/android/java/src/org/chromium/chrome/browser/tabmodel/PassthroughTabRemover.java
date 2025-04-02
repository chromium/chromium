// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

/**
 * Passthrough implementation of the {@link TabRemover} interface that forwards calls directly
 * through to {@link TabModel}.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public class PassthroughTabRemover implements TabRemover {
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    /**
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public PassthroughTabRemover(Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    @Override
    public void closeTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        prepareCloseTabs(tabClosureParams, allowDialog, listener, this::forceCloseTabs);
    }

    @Override
    public void prepareCloseTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener,
            Callback<TabClosureParams> onPreparedCallback) {
        if (listener != null) {
            listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        }
        onPreparedCallback.onResult(tabClosureParams);
        if (listener != null) {
            listener.onConfirmationDialogResult(
                    DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    @Override
    public void forceCloseTabs(TabClosureParams tabClosureParams) {
        TabGroupModelFilterInternal tabGroupModelFilter = getTabGroupModelFilter();
        doCloseTabs(tabGroupModelFilter, tabClosureParams);
    }

    @Override
    public void removeTab(Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
        if (listener != null) {
            listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        }
        TabGroupModelFilterInternal tabGroupModelFilter = getTabGroupModelFilter();
        doRemoveTab(tabGroupModelFilter.getTabModel(), tab);
        if (listener != null) {
            listener.onConfirmationDialogResult(
                    DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    private TabGroupModelFilterInternal getTabGroupModelFilter() {

        @Nullable TabGroupModelFilterInternal tabGroupModelFilter =
                (TabGroupModelFilterInternal) mTabGroupModelFilterSupplier.get();
        assert tabGroupModelFilter != null;
        return tabGroupModelFilter;
    }

    static boolean doCloseTabs(
            TabGroupModelFilterInternal filter, TabClosureParams tabClosureParams) {
        return filter.closeTabs(tabClosureParams);
    }

    static void doRemoveTab(TabModel model, Tab tab) {
        ((TabModelInternal) model).removeTab(tab);
    }
}
