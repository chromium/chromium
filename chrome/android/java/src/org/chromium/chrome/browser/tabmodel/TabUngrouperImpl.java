// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.util.List;

/** Implementation of the {@link TabUngrouper} interface. */
public class TabUngrouperImpl extends TabModelActor implements TabUngrouper {
    /**
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabUngrouperImpl(@NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        super(tabGroupModelFilterSupplier);
    }

    @Override
    public void ungroupTabs(
            @NonNull List<Tab> tabs,
            boolean trailing,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {
        // TODO(crbug.com/347981662, crbug.com/347981397, crbug.com/345854441): Show a dialog and
        // create NTPs if needed.
        TabGroupModelFilter filter = getTabGroupModelFilter();
        for (Tab tab : tabs) {
            filter.moveTabOutOfGroupInDirection(tab.getId(), trailing);
        }
        if (listener != null) {
            listener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }
}
