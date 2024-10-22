// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Base class for performing tab model actions. */
public abstract class TabModelActor {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    private @Nullable ActionConfirmationManager mActionConfirmationManager;

    /**
     * @param context The activity context.
     * @param modalDialogManager The manager to use for warning dialogs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabModelActor(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    protected @NonNull ActionConfirmationManager getActionConfirmationManager() {
        if (mActionConfirmationManager == null) {
            TabGroupModelFilter filter = getTabGroupModelFilter();
            mActionConfirmationManager =
                    new ActionConfirmationManager(
                            filter.getTabModel().getProfile(),
                            mContext,
                            filter,
                            mModalDialogManager);
        }
        return mActionConfirmationManager;
    }

    protected @NonNull TabGroupModelFilter getTabGroupModelFilter() {
        assert mTabGroupModelFilterSupplier.hasValue();
        return mTabGroupModelFilterSupplier.get();
    }
}
