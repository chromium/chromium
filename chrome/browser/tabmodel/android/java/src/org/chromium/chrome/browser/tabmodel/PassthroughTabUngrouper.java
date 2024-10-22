// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.util.List;

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
        assert mTabGroupModelFilterSupplier.hasValue();
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        doUngroupTabs(filter, tabs, trailing);
        if (listener != null) {
            listener.onConfirmationDialogResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        }
    }

    static void doUngroupTabs(
            @NonNull TabGroupModelFilter filter, @NonNull List<Tab> tabs, boolean trailing) {
        for (Tab tab : tabs) {
            filter.moveTabOutOfGroupInDirection(tab.getId(), trailing);
        }
    }
}
