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
    private static final long LOAD_TIMEOUT_MS = 5000L;

    private final Handler mHandler;
    private final ObservableSupplier<ModalDialogManager> mDialogManagerSupplier;

    private ModalDialogManager mDialogManager;
    private PropertyModel mModel;

    private long mShownAtMs;

    private @LoadingModalDialogCoordinator.State int mState;
    private boolean mSkipDelay;
    private boolean mDisableTimeout;

    /** ModalDialogProperties.Controller implementation */
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        dismissDialogImmediately(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogManager.removeObserver(this);
        mHandler.removeCallbacksAndMessages(null);
        mState = getFinalStateByDismissalCause(dismissalCause);
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
        mState = LoadingModalDialogCoordinator.State.LOADING_SHOWN;
        mShownAtMs = Long.valueOf(SystemClock.elapsedRealtime());
    }

    LoadingModalDialogMediator(
            ObservableSupplier<ModalDialogManager> dialogManagerSupplier, Handler handler) {
        assert dialogManagerSupplier != null;
        assert handler != null;
        mDialogManagerSupplier = dialogManagerSupplier;
        mState = LoadingModalDialogCoordinator.State.READY;
        mHandler = handler;
    }

    /**
     * Schedules the dialog to be shown after {@link #SHOW_DELAY_TIME_MS} milliseconds.
     * The dialog will not be shown if {@link #dismissDialog()} called before it become visible.
     *
     * @param model The {@link PropertyModel} describing the dialog to be shown.
     *
     */
    void showDialog(PropertyModel model) {
        assert mState == LoadingModalDialogCoordinator.State.READY;

        ModalDialogManager dialogManager = mDialogManagerSupplier.get();
        if (dialogManager == null) return;

        mDialogManager = dialogManager;
        mModel = model;
        mState = LoadingModalDialogCoordinator.State.LOADING_DELAYED;
        postDelayed(this::showDialogImmediately, SHOW_DELAY_TIME_MS);

        if (mDisableTimeout) return;
        Runnable timeoutDismissRunnable =
                () -> dismissDialogWithCause(DialogDismissalCause.CLIENT_TIMEOUT);
        postDelayed(timeoutDismissRunnable, LOAD_TIMEOUT_MS);
    }

    /**
     * Dismisses the currently visible dialog or cancelling the pending dialog if it is not visible
     * yet. If dialog is already visible for at least {@link #MINIMUM_SHOW_TIME_MS}, it will be
     * dismissed immediately. Otherwise it will be dismissed after being visible for that period of
     * time. This method should be called when the loading finishes.
     */
    void dismissDialog() {
        dismissDialogWithCause(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
    }

    /**
     * Indicates the current dialog state.
     */
    @LoadingModalDialogCoordinator.State
    int getState() {
        return mState;
    }

    void skipDelays() {
        mSkipDelay = true;
    }

    void disableTimeout() {
        mDisableTimeout = true;
    }

    private void dismissDialogWithCause(@DialogDismissalCause int dismissalCause) {
        if (mState != LoadingModalDialogCoordinator.State.LOADING_DELAYED
                && mState != LoadingModalDialogCoordinator.State.LOADING_SHOWN) {
            return;
        }

        mHandler.removeCallbacksAndMessages(null);

        final long currentTimeMs = SystemClock.elapsedRealtime();
        if (mState == LoadingModalDialogCoordinator.State.LOADING_SHOWN
                && mShownAtMs + MINIMUM_SHOW_TIME_MS > currentTimeMs) {
            // Dialog dismiss should be postponed to prevent UI flicker.
            switch (dismissalCause) {
                case DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED:
                    mState = LoadingModalDialogCoordinator.State.FINISHED_SHOWN;
                    break;
                case DialogDismissalCause.CLIENT_TIMEOUT:
                    mState = LoadingModalDialogCoordinator.State.TIMEOUT_SHOWN;
                    break;
                default:
                    assert false : "Unexpected dismissal cause: " + dismissalCause;
                    break;
            }
            Runnable dismissRunnable = () -> dismissDialogImmediately(dismissalCause);
            postDelayed(dismissRunnable, mShownAtMs + MINIMUM_SHOW_TIME_MS - currentTimeMs);
        } else {
            // Dialog is not yet shown or has been visible long enough.
            dismissDialogImmediately(dismissalCause);
        }
    }

    /** Immediately shows the dialog. */
    private void showDialogImmediately() {
        if (mState != LoadingModalDialogCoordinator.State.LOADING_DELAYED) return;
        mDialogManager.addObserver(this);
        mDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Immediately dismisses the dialog.
     *
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialog is
     *                       dismissed.
     */
    private void dismissDialogImmediately(@DialogDismissalCause int dismissalCause) {
        if (!canBeImmediatelyDismissed()) return;
        mDialogManager.dismissDialog(mModel, dismissalCause);
    }

    private void postDelayed(Runnable r, long delay) {
        if (mSkipDelay) {
            r.run();
        } else {
            mHandler.postDelayed(r, delay);
        }
    }

    private boolean canBeImmediatelyDismissed() {
        switch (mState) {
            case LoadingModalDialogCoordinator.State.LOADING_DELAYED:
            case LoadingModalDialogCoordinator.State.LOADING_SHOWN:
            case LoadingModalDialogCoordinator.State.FINISHED_SHOWN:
            case LoadingModalDialogCoordinator.State.TIMEOUT_SHOWN:
                return true;
            case LoadingModalDialogCoordinator.State.READY:
            case LoadingModalDialogCoordinator.State.FINISHED:
            case LoadingModalDialogCoordinator.State.CANCELLED:
                return false;
        }
        throw new AssertionError();
    }

    private @LoadingModalDialogCoordinator.State int getFinalStateByDismissalCause(
            @DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED:
                return LoadingModalDialogCoordinator.State.FINISHED;
            case DialogDismissalCause.CLIENT_TIMEOUT:
                return LoadingModalDialogCoordinator.State.TIMEOUT;
            default:
                return LoadingModalDialogCoordinator.State.CANCELLED;
        }
    }
}
