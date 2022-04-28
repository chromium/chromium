// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.RelativeLayout;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator class for displaying the loading modal dialog.
 * It handles the communication with the {@link ModalDialogManager}.
 * */
public class LoadingModalDialogCoordinator {
    private final RelativeLayout mCustomView;
    private final Context mContext;

    private PropertyModel mDialogModel;
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    /**
     * Creates the {@link LoadingModalDialogCoordinator}.
     *
     * @param modalDialogManager The ModalDialogManager which is going to display the dialog.
     * @param context The context for accessing resources.
     */
    public static LoadingModalDialogCoordinator create(
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier, Context context) {
        RelativeLayout dialogView =
                (RelativeLayout) LayoutInflater.from(context).inflate(R.layout.loading_modal, null);
        return new LoadingModalDialogCoordinator(context, modalDialogManagerSupplier, dialogView);
    }

    /**
     * Internal constructor for {@link LoadingModalDialogCoordinator}.
     *
     * @param context The context for accessing resources.
     * @param modalDialogManager The ModalDialogManager to display the dialog.
     * @param dialogView The custom view with dialog content.
     */
    private LoadingModalDialogCoordinator(Context context,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull RelativeLayout dialogView) {
        mContext = context;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mCustomView = dialogView;
    }

    /** Shows the loading modal dialog. */
    public void show() {
        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        if (modalDialogManager == null) return;
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.FULLSCREEN_DIALOG, true)
                        .with(ModalDialogProperties.CONTROLLER, new LoadingModalDialogMediator())
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                        .build();
        modalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }
}
