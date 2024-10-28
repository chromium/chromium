// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Implementation of the {@link TabRemover} interface. */
public class TabRemoverImpl implements TabRemover {
    private final TabModelRemover mTabModelRemover;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabRemoverImpl(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        this(new TabModelRemover(context, modalDialogManager, tabGroupModelFilterSupplier));
    }

    @VisibleForTesting
    TabRemoverImpl(@NonNull TabModelRemover tabModelRemover) {
        mTabModelRemover = tabModelRemover;
    }

    @Override
    public void closeTabs(
            @NonNull TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        // TODO(crbug.com/347981662, crbug.com/347981397, crbug.com/345854441): Show a dialog and
        // create NTPs if needed.
        PassthroughTabRemover.doCloseTabs(
                mTabModelRemover.getTabGroupModelFilter(), tabClosureParams);
        if (listener != null) {
            listener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    @Override
    public void forceCloseTabs(@NonNull TabClosureParams tabClosureParams) {
        PassthroughTabRemover.doCloseTabs(
                mTabModelRemover.getTabGroupModelFilter(), tabClosureParams);
    }

    @Override
    public void removeTab(
            @NonNull Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
        // TODO(crbug.com/347981662, crbug.com/347981397, crbug.com/345854441): Show a dialog and
        // create NTPs if needed.
        PassthroughTabRemover.doRemoveTab(
                mTabModelRemover.getTabGroupModelFilter().getTabModel(), tab);
        if (listener != null) {
            listener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }
}
