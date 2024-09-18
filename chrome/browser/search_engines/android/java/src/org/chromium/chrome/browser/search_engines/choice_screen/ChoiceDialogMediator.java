// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEnginesFeatureUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class ChoiceDialogMediator {
    @IntDef({
        DialogType.UNKNOWN,
        DialogType.LOADING,
        DialogType.CHOICE_LAUNCH,
        DialogType.CHOICE_CONFIRM
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DialogType {
        int UNKNOWN = 0;
        int LOADING = 1;
        int CHOICE_LAUNCH = 2;
        int CHOICE_CONFIRM = 3;
    }

    /** See {@link #startObserving}. */
    interface Delegate {
        /** Rebuilds the view tp match the requested {@code dialogType}. */
        void updateDialogType(@DialogType int dialogType);

        /** Triggers the dialog to be shown. */
        void showDialog();

        /** Dismisses the dialog, whether it's currently shown or pending to be shown. */
        void dismissDialog();

        /**
         * To be called when the mediator is getting destroyed. It does not want to get new updates
         * about clicks, dialog events, etc.
         */
        void onMediatorDestroyed();
    }

    private static final String TAG = "ChoiceDialogMediator";

    private final SearchEngineChoiceService mSearchEngineChoiceService;
    private final ObservableSupplier<Boolean> mIsDeviceChoiceRequiredSupplier;
    private final Callback<Boolean> mIsDeviceChoiceRequiredObserver;

    private @DialogType int mDialogType = DialogType.UNKNOWN;

    /**
     * Either the time at which the blocking dialog was shown, {@code null} indicating that the
     * dialog was not shown yet, or {@link Long#MIN_VALUE} indicating that the dialog has been
     * dismissed.
     */
    private @Nullable Long mDialogAddedTimeMillis;

    /**
     * Either the time at which observing the service started, or {@code null} if it didn't happen
     * yet.
     */
    private @Nullable Long mObservationStartedTimeMillis;

    /**
     * Either the time at which the first service event was received, or {@code null} if it didn't
     * happen yet.
     */
    private @Nullable Long mFirstServiceEventTimeMillis;

    private @Nullable Delegate mDelegate;

    /**
     * Constructs the mediator for the device choice dialog. To become active and start piloting the
     * dialog, call {@link #startObserving}.
     *
     * @param searchEngineChoiceService The service backing the dialog. It is used to determine
     *     whether it needs to be shown, process user actions, etc.
     */
    ChoiceDialogMediator(SearchEngineChoiceService searchEngineChoiceService) {
        mSearchEngineChoiceService = searchEngineChoiceService;
        mIsDeviceChoiceRequiredSupplier =
                searchEngineChoiceService.getIsDeviceChoiceRequiredSupplier();

        // Need to store the lambda reference. As it changes on subsequent calls, it would otherwise
        // be impossible to remove the observer.
        mIsDeviceChoiceRequiredObserver = this::onIsDeviceChoiceRequiredChanged;
    }

    /**
     * Makes the dialog subscribe to changes from the service.
     *
     * @param delegate processes state changes communicated by the mediator and updates the state of
     *     the UI.
     */
    void startObserving(@NonNull Delegate delegate) {
        assert mDelegate == null;
        mDelegate = delegate;

        mObservationStartedTimeMillis = System.currentTimeMillis();
        mDialogType = DialogType.LOADING;

        if (!mIsDeviceChoiceRequiredSupplier.hasValue()) {
            // An initial response from the supplier is still pending, so it won't call the observer
            // on registration by itself. It's unclear how long it would take. We proactively
            // trigger the blocking dialog, but if it takes too long we will unblock the user.
            // We do it asynchronously to match how it is done via the supplier when it has a value.
            ThreadUtils.postOnUiThread(
                    () -> {
                        mDelegate.updateDialogType(DialogType.LOADING);
                        mDelegate.showDialog();
                    });
        }
        mIsDeviceChoiceRequiredSupplier.addObserver(mIsDeviceChoiceRequiredObserver);
    }

    private void destroy() {
        if (mDelegate == null) return;

        // Prevent re-entry.
        var delegate = mDelegate;
        mDelegate = null;

        mIsDeviceChoiceRequiredSupplier.removeObserver(mIsDeviceChoiceRequiredObserver);
        mDialogType = DialogType.UNKNOWN;

        delegate.onMediatorDestroyed();
    }

    /**
     * Method to call when the primary action button of the dialog is tapped.
     *
     * @param dialogType type of the dialog at the moment the button was wired up.
     */
    void onActionButtonClick(@DialogType int dialogType) {
        assert mDelegate != null;

        switch (dialogType) {
            case DialogType.CHOICE_LAUNCH -> mSearchEngineChoiceService.launchDeviceChoiceScreens();
            case DialogType.CHOICE_CONFIRM -> mDelegate.dismissDialog();
            case DialogType.LOADING, DialogType.UNKNOWN -> throw new IllegalStateException();
        }
    }

    /** Method to call when the dialog is actually shown. */
    void onDialogAdded() {
        assert mDialogAddedTimeMillis == null
                : "The dialog is not expected to have already been shown";
        assert mDialogType != DialogType.UNKNOWN;
        assert mObservationStartedTimeMillis != null;
        mDialogAddedTimeMillis = System.currentTimeMillis();
        mSearchEngineChoiceService.notifyDeviceChoiceBlockShown();

        // TODO(b/355201070): Replace this after e2e testing with UMA recording.
        Log.i(
                TAG,
                "onDialogAdded(), time since observation start: %s millis",
                mDialogAddedTimeMillis - mObservationStartedTimeMillis);
        scheduleDismissOnDeviceChoiceRequiredUpdateTimeout();
    }

    void onDialogDismissed() {
        destroy();
    }

    @MainThread
    private void onIsDeviceChoiceRequiredChanged(@Nullable Boolean isDeviceChoiceRequired) {
        ThreadUtils.checkUiThread();

        assert mDelegate != null;
        boolean wasDialogShown = mDialogAddedTimeMillis != null;
        boolean wasDialogDismissed = wasDialogShown && mDialogType == DialogType.UNKNOWN;

        if (mFirstServiceEventTimeMillis == null) {
            mFirstServiceEventTimeMillis = System.currentTimeMillis();
            // TODO(b/355201070): Replace this after e2e testing with UMA recording.
            Log.i(
                    TAG,
                    "onIsDeviceChoiceRequiredChanged(%s), time since dialog added: %s millis, "
                            + "time since observation started: %s millis",
                    isDeviceChoiceRequired,
                    wasDialogShown
                            ? mFirstServiceEventTimeMillis - mDialogAddedTimeMillis
                            : "<N/A>",
                    mObservationStartedTimeMillis != null
                            ? mFirstServiceEventTimeMillis - mObservationStartedTimeMillis
                            : "<N/A>");
        }

        if (Boolean.TRUE.equals(isDeviceChoiceRequired) && !wasDialogDismissed) {
            mDialogType = DialogType.CHOICE_LAUNCH;
            mDelegate.updateDialogType(DialogType.CHOICE_LAUNCH);

            if (!wasDialogShown) {
                mDelegate.showDialog();
            }
            return;
        }

        // `isDeviceChoiceRequired` being null indicates that the backend was disconnected, and
        // false indicates that blocking the user is not necessary anymore. In both cases we'll
        // want to unblock the user, but based on which state the UI is in, we may show some
        // confirmation message or not.

        if (wasDialogShown && !wasDialogDismissed) {
            if (Boolean.FALSE.equals(isDeviceChoiceRequired)
                    && (mDialogType == DialogType.LOADING
                            || mDialogType == DialogType.CHOICE_LAUNCH)) {
                // This is the normal flow, showing confirmation after the choice has been made.
                mDialogType = DialogType.CHOICE_CONFIRM;
                mDelegate.updateDialogType(DialogType.CHOICE_CONFIRM);
                mSearchEngineChoiceService.notifyDeviceChoiceBlockCleared();
                return;
            }

            if (mDialogType == DialogType.CHOICE_CONFIRM) {
                // The backend is sending us some updates while we are showing the confirmation UI.
                // We are not blocking and the user can proceed, so don't do anything about it.
                return;
            }
        }

        // If we get here, this is some sort of error state. Shutdown everything.
        // Indicates that the backend was disconnected. This would make the dialog non-functional if
        // it is still shown, so let's dismiss it and let the user proceed to Chrome.
        // TODO(b/355201070): Add UMA recording, remove or update the log below.
        Log.w(
                TAG,
                "Unexpected backend update received. State: "
                        + "{wasDialogShown=%b, wasDialogDismissed=%b, mDialogType=%s, "
                        + "isDeviceChoiceRequired=%s}",
                wasDialogShown,
                wasDialogDismissed,
                mDialogType,
                isDeviceChoiceRequired);
        mDelegate.dismissDialog();
        destroy();
    }

    private void scheduleDismissOnDeviceChoiceRequiredUpdateTimeout() {
        if (mDialogType != DialogType.LOADING) {
            return;
        }

        int dialogTimeoutMillis = SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis();
        if (dialogTimeoutMillis > 0) {
            ThreadUtils.postOnUiThreadDelayed(
                    () -> {
                        if (mDialogType != DialogType.LOADING) {
                            return; // No-op, we got an update.
                        }

                        assert mDelegate != null; // Unexpected if the type is still "loading".

                        Log.w(
                                TAG,
                                "Timeout waiting for backend block confirmation. Deadline: %s ms",
                                dialogTimeoutMillis);

                        mDelegate.dismissDialog();
                        destroy();
                    },
                    dialogTimeoutMillis);
        }
    }
}
