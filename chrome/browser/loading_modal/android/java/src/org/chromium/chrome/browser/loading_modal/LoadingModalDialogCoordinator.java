// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
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
    @IntDef({
        State.READY,
        State.PENDING,
        State.SHOWN,
        State.FINISHED,
        State.CANCELLED,
        State.TIMED_OUT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        /** Loading is not started, the dialog is not shown. */
        int READY = 0;

        /** The dialog is scheduled to be shown after the default delay. */
        int PENDING = 1;

        /** The dialog is visible. */
        int SHOWN = 2;

        /**
         * Dialog is dismissed by the client as the loading operation finished. It may be still
         * visible for a short period to prevent UI flickering.
         */
        int FINISHED = 3;

        /** User dismissed the dialog before the loading finished. */
        int CANCELLED = 4;

        /** Loading timeout occurred and the dialog was automatically dismissed. */
        int TIMED_OUT = 5;

        int NUM_ENTRIES = 6;
    }

    /**
     * An observer of the LoadingModalDialogCoordinator intended to broadcast notifications
     * about the loading dialog dismissals and the readiness to be immediately dismissed.
     */
    public interface Observer {
        /** A notification that the dialog could be dismissed without causing the UI to flicker. */
        default void onDismissable() {}

        /** A notification that the dialog was dismissed with given final state. */
        default void onDismissedWithState(@State int finalState) {}
    }

    /**
     * Creates the {@link LoadingModalDialogCoordinator}.
     *
     * @param modalDialogManagerSupplier The supplier of the ModalDialogManager which is going to
     *         display the dialog.
     * @param context The context for accessing resources.
     */
    public static LoadingModalDialogCoordinator create(
            Supplier<ModalDialogManager> modalDialogManagerSupplier, Context context) {
        return create(modalDialogManagerSupplier, context, new Handler(Looper.getMainLooper()));
    }

    @VisibleForTesting
    static LoadingModalDialogCoordinator create(
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Context context,
            Handler handler) {
        LoadingModalDialogMediator dialogMediator =
                new LoadingModalDialogMediator(modalDialogManagerSupplier, handler);
        RelativeLayout dialogView =
                (RelativeLayout) LayoutInflater.from(context).inflate(R.layout.loading_modal, null);
        RelativeLayout buttonsView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.loading_modal_button_bar, null);
        return new LoadingModalDialogCoordinator(dialogMediator, dialogView, buttonsView);
    }

    /**
     * Internal constructor for {@link LoadingModalDialogCoordinator}.
     *
     * @param modalDialogMediator The LoadingModalDialogMediator to control the dialog.
     * @param dialogView The custom view with dialog content.
     */
    private LoadingModalDialogCoordinator(
            @NonNull LoadingModalDialogMediator dialogMediator,
            @NonNull RelativeLayout dialogView,
            @Nullable View buttonsView) {
        mMediator = dialogMediator;
        mCustomView = dialogView;
        mButtonsView = buttonsView;
    }

    /**
     * Schedules the dialog to be shown after delay. The dialog will not be shown if
     * {@link #finishLoading()} called before it become visible.
     */
    public void show() {
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.FULLSCREEN_DIALOG)
                        .with(ModalDialogProperties.CONTROLLER, mMediator)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                        .with(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, mButtonsView)
                        .build();
        mButtonsView
                .findViewById(R.id.cancel_loading_modal)
                .setOnClickListener(view -> mMediator.onClick(dialogModel, ButtonType.NEGATIVE));
        mMediator.show(dialogModel);
    }

    /**
     * Dismisses the currently visible dialog or cancelling the pending dialog if it is not visible
     * yet. If dialog is already visible for at least {@link #MINIMUM_SHOW_TIME_MS}, it will be
     * dismissed immediately. Otherwise it will be dismissed after being visible for that period of
     * time.
     */
    public void dismiss() {
        mMediator.dismiss();
    }

    /** Indicates the current dialog state. */
    public @State int getState() {
        return mMediator.getState();
    }

    void skipDelayForTesting() {
        mMediator.skipDelays();
    }

    void disableTimeoutForTesting() {
        mMediator.disableTimeout();
    }

    @VisibleForTesting
    View getButtonsView() {
        return mButtonsView;
    }

    /** Indicates if the dailog could be immediately dismissed. */
    public boolean isImmediatelyDismissable() {
        return mMediator.isImmediatelyDismissable();
    }

    /**
     * Add the listener that will be notified when the dialog is cancelled, timed out or is ready to
     * be dismissed.
     *
     * @param listener {@link Observer} that will be notified.
     */
    public void addObserver(Observer listener) {
        mMediator.addObserver(listener);
    }
}
