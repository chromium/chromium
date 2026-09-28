// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.util.List;
import java.util.function.Supplier;

/**
 * Passthrough implementation of the {@link TabRemover} interface that forwards calls directly
 * through to {@link TabModel}.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public class PassthroughTabRemover implements TabRemover {
    private final Supplier<@Nullable TabModel> mTabModelSupplier;
    private final TabClosePrompter mTabClosePrompter;

    /**
     * @param tabModelSupplier The supplier of the {@link TabModel}.
     */
    public PassthroughTabRemover(Supplier<@Nullable TabModel> tabModelSupplier) {
        this(tabModelSupplier, new BeforeUnloadTabClosePrompter());
    }

    @VisibleForTesting
    PassthroughTabRemover(
            Supplier<@Nullable TabModel> tabModelSupplier, TabClosePrompter tabClosePrompter) {
        mTabModelSupplier = tabModelSupplier;
        mTabClosePrompter = tabClosePrompter;
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
        Callback<TabClosureParams> proceedWithClose =
                params -> {
                    if (listener != null) {
                        listener.willPerformActionOrShowDialog(
                                DialogType.NONE, /* willSkipDialog= */ true);
                    }
                    onPreparedCallback.onResult(params);
                    if (listener != null) {
                        listener.onConfirmationDialogResult(
                                DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
                    }
                };

        if (!allowDialog) {
            proceedWithClose.onResult(tabClosureParams);
            return;
        }

        List<Tab> tabsToClose =
                tabClosureParams.isAllTabs
                        ? TabModelUtils.convertTabListToListOfTabs(getTabModel())
                        : tabClosureParams.tabs;
        if (tabsToClose == null) {
            proceedWithClose.onResult(tabClosureParams);
            return;
        }

        TabRemover.checkBeforeUnloadAndProceed(
                tabClosureParams,
                tabsToClose,
                mTabClosePrompter,
                /* abandonBatchOnCancel= */ tabClosureParams.isAllTabs,
                listener,
                confirmedTabs ->
                        TabRemover.proceedWithConfirmedTabs(
                                tabClosureParams,
                                getTabModel(),
                                tabsToClose,
                                confirmedTabs,
                                listener,
                                proceedWithClose));
    }

    @Override
    public void forceCloseTabs(TabClosureParams tabClosureParams) {
        doCloseTabs(getTabModel(), tabClosureParams);
    }

    @Override
    public void removeTab(Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
        if (listener != null) {
            listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        }
        doRemoveTab(getTabModel(), tab);
        if (listener != null) {
            listener.onConfirmationDialogResult(
                    DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    private TabModel getTabModel() {
        TabModel tabModel = mTabModelSupplier.get();
        assert tabModel != null;
        return tabModel;
    }

    static boolean doCloseTabs(TabModel tabModel, TabClosureParams tabClosureParams) {
        return ((TabModelInternal) tabModel).closeTabs(tabClosureParams);
    }

    static void doRemoveTab(TabModel model, Tab tab) {
        ((TabModelInternal) model).removeTab(tab);
    }
}
