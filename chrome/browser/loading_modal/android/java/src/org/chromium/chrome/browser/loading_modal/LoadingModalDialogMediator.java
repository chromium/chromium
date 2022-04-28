// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import android.os.Handler;
import android.os.SystemClock;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class responsible for handling button clicks and controlling the dialog state.
 *
 * Prevents UI flickering by following the hiding/showing policy defined in Material Design.
 */
class LoadingModalDialogMediator
        implements ModalDialogProperties.Controller, ModalDialogManagerObserver {
    private static final long SHOW_DELAY_TIME_MS = 500L;
    private static final long MINIMUM_SHOW_TIME_MS = 500L;

    private final Handler mHandler = new Handler();
    private final ObservableSupplier<ModalDialogManager> mDialogManagerSupplier;

    private ModalDialogManager mDialogManager;
    private PropertyModel mModel;

    /**
     * Tracks whether the Dialog should be displayed when {@link #showDialogImmediately()} is run.
     * Android doesn't always cancel a Runnable when requested, meaning that the Dialog could be
     * hidden before it even has a chance to be shown.
     */
    private boolean mShouldShow;
    private Long mShownAtMs;

    /** ModalDialogProperties.Controller implementation */
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogManager.removeObserver(this);
    }

    /**
     * ModalDialogManagerObserver implementation.
     *
     * Dialog could be postponed if there is a dialog with higher priority.
     * We track the actual time when the dialog become visible to keep it for a desired period of
     * time.
     *
     * Dialog manager calls {@link #onDismiss()} on dialog dismissal, no need to handle it here.
     */
    @Override
    public void onDialogAdded(PropertyModel model) {
        if (model != mModel) return;
        mShownAtMs = Long.valueOf(SystemClock.elapsedRealtime());
    }

    LoadingModalDialogMediator(ObservableSupplier<ModalDialogManager> dialogManagerSupplier) {
        assert dialogManagerSupplier != null;
        mDialogManagerSupplier = dialogManagerSupplier;
    }

    /**
     * Schedules the dialog to be shown after {@link #SHOW_DELAY_TIME_MS} milliseconds.
     * The dialog will not be shown if {@link #dismissDialog()} called before it become visible.
     *
     * @param model The {@link PropertyModel} describing the dialog to be shown.
     *
     */
    void showDialog(PropertyModel model) {
        assert mModel == null : "dialog is already visible or pending";

        ModalDialogManager dialogManager = mDialogManagerSupplier.get();
        if (dialogManager == null) return;

        mDialogManager = dialogManager;
        mModel = model;
        mShouldShow = true;
        mHandler.postDelayed(this::showDialogImmediately, SHOW_DELAY_TIME_MS);
    }

    /**
     * Dismisses the currently visible dialog or cancelling the pending dialog if it is not visible
     * yet. If dialog is already visible for at least {@link #MINIMUM_SHOW_TIME_MS}, it will be
     * dismissed immediately. Otherwise it will be dismissed after being visible for that period of
     * time.
     *
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialog is
     *                       dismissed.
     */
    void dismissDialog(@DialogDismissalCause int dismissalCause) {
        if (mModel == null) return;
        mShouldShow = false;

        mHandler.removeCallbacksAndMessages(null);

        final long currentTimeMs = SystemClock.elapsedRealtime();
        System.out.println("currentTimeMs = " + currentTimeMs);

        if (mShownAtMs != null && mShownAtMs.longValue() + MINIMUM_SHOW_TIME_MS > currentTimeMs) {
            // Dialog dismiss should be postponed to prevent UI flicker.
            Runnable dismissRunnable = () -> dismissDialogImmediately(dismissalCause);
            mHandler.postDelayed(
                    dismissRunnable, mShownAtMs.longValue() + MINIMUM_SHOW_TIME_MS - currentTimeMs);
        } else {
            // Dialog is not yet shown or has been visible long enough.
            dismissDialogImmediately(dismissalCause);
        }
    }

    /** Immediately shows the dialog. */
    private void showDialogImmediately() {
        if (!mShouldShow) return;
        mDialogManager.addObserver(this);
        mDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /** Immediately dismisses the dialog. */
    private void dismissDialogImmediately(@DialogDismissalCause int dismissalCause) {
        if (mShouldShow) return;
        mDialogManager.dismissDialog(mModel, dismissalCause);
    }
}
