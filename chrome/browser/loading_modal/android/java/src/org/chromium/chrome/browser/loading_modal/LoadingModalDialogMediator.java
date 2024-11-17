// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import android.os.Handler;
import android.os.SystemClock;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
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
    // The load timeout limits the dialog visibility period, along with the show delay time it
    // results in 5000ms total loading timeout.
    private static final long LOAD_TIMEOUT_MS = 4500L;

    private final Handler mHandler;
    private final Supplier<ModalDialogManager> mDialogManagerSupplier;
    private final ObserverList<LoadingModalDialogCoordinator.Observer> mObservers =
            new ObserverList<>();

    private ModalDialogManager mDialogManager;
    private PropertyModel mModel;

    private long mShownAtMs;

    private @LoadingModalDialogCoordinator.State int mState;
    private boolean mSkipDelay;
    private boolean mDisableTimeout;

    private Runnable mShowingTask = this::onShowDelayPassed;

    /** ModalDialogProperties.Controller implementation */
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        mState = LoadingModalDialogCoordinator.State.CANCELLED;
        dismissDialogWithCause(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogManager.removeObserver(this);
        mHandler.removeCallbacksAndMessages(null);
        mState = getFinalStateByDismissalCause(dismissalCause);
        if (mState == LoadingModalDialogCoordinator.State.CANCELLED
                || mState == LoadingModalDialogCoordinator.State.TIMED_OUT) {
            for (LoadingModalDialogCoordinator.Observer observer : mObservers) {
                observer.onDismissedWithState(mState);
            }
        }
        mObservers.clear();
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
        mState = LoadingModalDialogCoordinator.State.SHOWN;
        mShownAtMs = SystemClock.elapsedRealtime();
        postDelayed(this::onDismissDelayPassed, MINIMUM_SHOW_TIME_MS);
        if (!mDisableTimeout) postDelayed(this::onTimeoutOccurred, LOAD_TIMEOUT_MS);
    }

    LoadingModalDialogMediator(
            Supplier<ModalDialogManager> dialogManagerSupplier, Handler handler) {
        assert dialogManagerSupplier != null;
        assert handler != null;
        mDialogManagerSupplier = dialogManagerSupplier;
        mState = LoadingModalDialogCoordinator.State.READY;
        mHandler = handler;
    }

    /**
     * Add the listener that will be notified about the loading dialog cancellation and readiness to
     * be immediately dismissed.
     *
     * @param listener {@link LoadingModalDialogCoordinator.Observer} that will be notified.
     */
    void addObserver(LoadingModalDialogCoordinator.Observer listener) {
        mObservers.addObserver(listener);
    }

    /**
     * Schedules the dialog to be shown after {@link #SHOW_DELAY_TIME_MS} milliseconds.
     * The dialog will not be shown if {@link #dismiss()} called before it become visible.
     *
     * @param model The {@link PropertyModel} describing the dialog to be shown.
     *
     */
    void show(PropertyModel model) {
        assert mState == LoadingModalDialogCoordinator.State.READY;

        ModalDialogManager dialogManager = mDialogManagerSupplier.get();
        if (dialogManager == null) return;

        mDialogManager = dialogManager;
        mModel = model;
        mState = LoadingModalDialogCoordinator.State.PENDING;
        postDelayed(mShowingTask, SHOW_DELAY_TIME_MS);
    }

    /**
     * Dismisses the currently visible dialog or cancelling the pending dialog if it is not visible
     * yet. If dialog is already visible for at least {@link #MINIMUM_SHOW_TIME_MS}, it will be
     * dismissed immediately. Otherwise it will be dismissed after being visible for that period of
     * time.
     */
    void dismiss() {
        if (mState == LoadingModalDialogCoordinator.State.PENDING) {
            mHandler.removeCallbacks(mShowingTask);
        }

        mState = LoadingModalDialogCoordinator.State.FINISHED;
        if (isImmediatelyDismissable()) {
            dismissDialogWithCause(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        }
    }

    /** Indicates the current dialog state. */
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

    /** Indicates if the dailog could be immediately dismissed. */
    boolean isImmediatelyDismissable() {
        switch (mState) {
            case LoadingModalDialogCoordinator.State.PENDING:
            case LoadingModalDialogCoordinator.State.TIMED_OUT:
            case LoadingModalDialogCoordinator.State.CANCELLED:
                return true;
            case LoadingModalDialogCoordinator.State.SHOWN:
            case LoadingModalDialogCoordinator.State.FINISHED:
                return mSkipDelay
                        || SystemClock.elapsedRealtime() - mShownAtMs >= MINIMUM_SHOW_TIME_MS;
            case LoadingModalDialogCoordinator.State.READY:
                return false;
        }
        throw new AssertionError();
    }

    private void onShowDelayPassed() {
        if (mState == LoadingModalDialogCoordinator.State.PENDING) {
            showDialogImmediately();
        }
    }

    private void onDismissDelayPassed() {
        for (LoadingModalDialogCoordinator.Observer observer : mObservers) observer.onDismissable();
        if (mState == LoadingModalDialogCoordinator.State.FINISHED) {
            dismissDialogWithCause(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        }
    }

    private void onTimeoutOccurred() {
        if (mState != LoadingModalDialogCoordinator.State.SHOWN) return;

        mState = LoadingModalDialogCoordinator.State.TIMED_OUT;
        dismissDialogWithCause(DialogDismissalCause.CLIENT_TIMEOUT);
    }

    /** Immediately shows the dialog. */
    private void showDialogImmediately() {
        assert mState == LoadingModalDialogCoordinator.State.PENDING;
        mDialogManager.addObserver(this);
        mDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /**
     * Immediately dismisses the dialog with {@link DialogDismissalCause}.
     *
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialog is
     *                       dismissed.
     */
    private void dismissDialogWithCause(@DialogDismissalCause int dismissalCause) {
        assert isImmediatelyDismissable();
        mDialogManager.dismissDialog(mModel, dismissalCause);
    }

    private void postDelayed(Runnable r, long delay) {
        if (mSkipDelay) {
            r.run();
        } else {
            mHandler.postDelayed(r, delay);
        }
    }

    private @LoadingModalDialogCoordinator.State int getFinalStateByDismissalCause(
            @DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED:
                return LoadingModalDialogCoordinator.State.FINISHED;
            case DialogDismissalCause.CLIENT_TIMEOUT:
                return LoadingModalDialogCoordinator.State.TIMED_OUT;
            default:
                return LoadingModalDialogCoordinator.State.CANCELLED;
        }
    }
}
