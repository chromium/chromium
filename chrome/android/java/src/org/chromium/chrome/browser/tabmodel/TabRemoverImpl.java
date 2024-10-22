// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

/** Implementation of the {@link TabRemover} interface. */
public class TabRemoverImpl extends TabModelActor implements TabRemover {
    /**
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabRemoverImpl(@NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        super(tabGroupModelFilterSupplier);
    }

    @Override
    public void closeTabs(
            @NonNull TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        // TODO(crbug.com/347981662, crbug.com/347981397, crbug.com/345854441): Show a dialog and
        // create NTPs if needed.
        getTabGroupModelFilter().closeTabs(tabClosureParams);
        if (listener != null) {
            listener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    @Override
    public void forceCloseTabs(@NonNull TabClosureParams tabClosureParams) {
        getTabGroupModelFilter().closeTabs(tabClosureParams);
    }

    @Override
    public void removeTab(
            @NonNull Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
        // TODO(crbug.com/347981662, crbug.com/347981397, crbug.com/345854441): Show a dialog and
        // create NTPs if needed.
        getTabGroupModelFilter().getTabModel().removeTab(tab);
        if (listener != null) {
            listener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }
}
