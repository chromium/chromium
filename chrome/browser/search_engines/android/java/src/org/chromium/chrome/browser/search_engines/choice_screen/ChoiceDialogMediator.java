// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.components.search_engines.SearchEngineChoiceService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class ChoiceDialogMediator {
    @IntDef({DialogType.UNKNOWN, DialogType.CHOICE_LAUNCH, DialogType.CHOICE_CONFIRM})
    @Retention(RetentionPolicy.SOURCE)
    @interface DialogType {
        int UNKNOWN = 0;
        int CHOICE_LAUNCH = 1;
        int CHOICE_CONFIRM = 2;
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
    private boolean mIsDialogShown;

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
            case DialogType.UNKNOWN -> throw new IllegalStateException();
        }
    }

    /** Method to call when the dialog is actually shown. */
    void onDialogAdded() {
        mIsDialogShown = true;
        mSearchEngineChoiceService.notifyDeviceChoiceBlockShown();
    }

    void onDialogDismissed() {
        mIsDialogShown = false;
        destroy();
    }

    private void onIsDeviceChoiceRequiredChanged(@Nullable Boolean isDeviceChoiceRequired) {
        assert mDelegate != null;

        if (Boolean.TRUE.equals(isDeviceChoiceRequired)) {
            // We expect it to happen only as the very first notification we get. Other values as
            // first notification lead to skipping the dialog entirely.
            assert !mIsDialogShown;
            mDialogType = DialogType.CHOICE_LAUNCH;
            mDelegate.updateDialogType(DialogType.CHOICE_LAUNCH);
            mDelegate.showDialog();
            return;
        }

        // `isDeviceChoiceRequired` being null indicates that the backend was disconnected, and
        // false indicates that blocking the user is not necessary anymore. In both cases we'll want
        // to unblock the user, but based on which state the UI is in, we may show some confirmation
        // message or not.

        if (mIsDialogShown
                && Boolean.FALSE.equals(isDeviceChoiceRequired)
                && mDialogType == DialogType.CHOICE_LAUNCH) {
            // This is the normal flow, showing confirmation after the choice has been made.
            mDialogType = DialogType.CHOICE_CONFIRM;
            mDelegate.updateDialogType(DialogType.CHOICE_CONFIRM);
            mSearchEngineChoiceService.notifyDeviceChoiceBlockCleared();
            return;
        }

        if (mIsDialogShown && mDialogType == DialogType.CHOICE_CONFIRM) {
            // The backend is sending us some updates while we are showing the confirmation UI. We
            // are not blocking anyway and the user can proceed, so don't do anything about it.
            return;
        }

        // If we get here, this is some sort of error state. Shutdown everything.
        // Indicates that the backend was disconnected. This would make the dialog
        // non-functional, so let's dismiss it and let the user proceed to Chrome.
        // TODO(b/355201070): Add UMA recording.
        Log.w(
                TAG,
                "Unexpected backend update received. State: "
                        + "{mIsDialogShown=%b, mDialogType=%s, isDeviceChoiceRequired=%s}",
                mIsDialogShown,
                mDialogType,
                isDeviceChoiceRequired);
        mDelegate.dismissDialog();
        destroy();
    }
}
