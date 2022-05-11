// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinator class for displaying the loading modal dialog.
 * It proxies the communication to the {@link LoadingModalDialogMediator}.
 * */
public class LoadingModalDialogCoordinator {
    private final LoadingModalDialogMediator mMediator;
    private final RelativeLayout mCustomView;
    private final View mButtonsView;

    // Used to indicate the current loading dialog state.
    @IntDef({State.READY, State.LOADING_DELAYED, State.LOADING_SHOWN, State.FINISHED_SHOWN,
            State.FINISHED, State.CANCELLED, State.TIMEOUT_SHOWN, State.TIMEOUT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        /** Loading is not started, the dialog is not shown. */
        int READY = 0;
        /** Loading in progress, the dialog is delayed. */
        int LOADING_DELAYED = 1;
        /** Loading in progress, the dialog is visible. */
        int LOADING_SHOWN = 2;
        /** Loading finished, the dialog dismissal is delayed. */
        int FINISHED_SHOWN = 3;
        /** Loading finished, the dialog is dismissed. */
        int FINISHED = 4;
        /** User dismissed the dialog before the loading finished. */
        int CANCELLED = 5;
        /** Loading timeout occurred but the dialog is still visible to prevent flickering. */
        int TIMEOUT_SHOWN = 6;
        /** Loading timeout occurred and the dialog was automatically dismissed. */
        int TIMEOUT = 7;
        int NUM_ENTRIES = 8;
    }

    /**
     * Creates the {@link LoadingModalDialogCoordinator}.
     *
     * @param modalDialogManagerSupplier The supplier of the ModalDialogManager which is going to
     *         display the dialog.
     * @param context The context for accessing resources.
     */
    public static LoadingModalDialogCoordinator create(
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier, Context context) {
        return create(modalDialogManagerSupplier, context, new Handler());
    }

    @VisibleForTesting
    static LoadingModalDialogCoordinator create(
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier, Context context,
            Handler handler) {
        LoadingModalDialogMediator dialogMediator =
                new LoadingModalDialogMediator(modalDialogManagerSupplier, handler);
        RelativeLayout dialogView =
                (RelativeLayout) LayoutInflater.from(context).inflate(R.layout.loading_modal, null);
        RelativeLayout buttonsView = (RelativeLayout) LayoutInflater.from(context).inflate(
                R.layout.loading_modal_button_bar, null);
        return new LoadingModalDialogCoordinator(dialogMediator, dialogView, buttonsView);
    }

    /**
     * Internal constructor for {@link LoadingModalDialogCoordinator}.
     *
     * @param modalDialogMediator The LoadingModalDialogMediator to control the dialog.
     * @param dialogView The custom view with dialog content.
     */
    private LoadingModalDialogCoordinator(@NonNull LoadingModalDialogMediator dialogMediator,
            @NonNull RelativeLayout dialogView, @Nullable View buttonsView) {
        mMediator = dialogMediator;
        mCustomView = dialogView;
        mButtonsView = buttonsView;
    }

    /** Shows the loading modal dialog. */
    public void show() {
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.FULLSCREEN_DIALOG, true)
                        .with(ModalDialogProperties.EXCEED_MAX_HEIGHT, true)
                        .with(ModalDialogProperties.CONTROLLER, mMediator)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                        .with(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, mButtonsView)
                        .build();
        mButtonsView.findViewById(R.id.cancel_loading_modal)
                .setOnClickListener(view -> mMediator.onClick(dialogModel, ButtonType.NEGATIVE));
        mMediator.showDialog(dialogModel);
    }

    /** Dismisses the loading modal dialog. */
    public void dismiss() {
        mMediator.dismissDialog();
    }

    /**
     * Indicates the current dialog state.
     */
    public @State int getState() {
        return mMediator.getState();
    }

    @VisibleForTesting
    void skipDelayForTesting() {
        mMediator.skipDelays();
    }

    @VisibleForTesting
    void disableTimeoutForTesting() {
        mMediator.disableTimeout();
    }
    @VisibleForTesting
    View getButtonsView() {
        return mButtonsView;
    }
}
