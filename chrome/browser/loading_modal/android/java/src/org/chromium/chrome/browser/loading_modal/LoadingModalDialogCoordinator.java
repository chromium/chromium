// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.RelativeLayout;

import androidx.annotation.NonNull;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator class for displaying the loading modal dialog.
 * It handles the communication with the {@link ModalDialogManager}.
 * */
public class LoadingModalDialogCoordinator {
    private final ModalDialogManager mModalDialogManager;
    private final RelativeLayout mCustomView;
    private final Context mContext;

    private PropertyModel mDialogModel;

    /**
     * Creates the {@link LoadingModalDialogCoordinator}.
     *
     * @param modalDialogManager The ModalDialogManager which is going to display the dialog.
     * @param context The context for accessing resources.
     */
    public static LoadingModalDialogCoordinator create(
            ModalDialogManager modalDialogManager, Context context) {
        RelativeLayout dialogView =
                (RelativeLayout) LayoutInflater.from(context).inflate(R.layout.loading_modal, null);
        return new LoadingModalDialogCoordinator(context, modalDialogManager, dialogView);
    }

    /**
     * Internal constructor for {@link LoadingModalDialogCoordinator}.
     *
     * @param context The context for accessing resources.
     * @param modalDialogManager The ModalDialogManager to display the dialog.
     * @param dialogView The custom view with dialog content.
     */
    private LoadingModalDialogCoordinator(@NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager, @NonNull RelativeLayout dialogView) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mCustomView = dialogView;
    }

    /** Shows the loading modal dialog. */
    public void show() {
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.FULLSCREEN_DIALOG, true)
                        .with(ModalDialogProperties.CONTROLLER, new LoadingModalDialogMediator())
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                        .build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }
}
