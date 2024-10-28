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

import java.util.List;

/** Implementation of the {@link TabUngrouper} interface. */
public class TabUngrouperImpl implements TabUngrouper {
    private final TabModelRemover mTabModelRemover;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabUngrouperImpl(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        this(new TabModelRemover(context, modalDialogManager, tabGroupModelFilterSupplier));
    }

    @VisibleForTesting
    TabUngrouperImpl(@NonNull TabModelRemover tabModelRemover) {
        mTabModelRemover = tabModelRemover;
    }

    @Override
    public void ungroupTabs(
            @NonNull List<Tab> tabs,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        // TODO(crbug.com/347981662, crbug.com/347981397, crbug.com/345854441): Show a dialog and
        // create NTPs if needed.
        PassthroughTabUngrouper.doUngroupTabs(
                mTabModelRemover.getTabGroupModelFilter(), tabs, trailing);
        if (listener != null) {
            listener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }
}
