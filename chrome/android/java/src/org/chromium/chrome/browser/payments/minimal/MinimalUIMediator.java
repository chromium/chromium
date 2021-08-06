// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.minimal;

import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.fingerprint.FingerprintManager;
import android.hardware.fingerprint.FingerprintManager.AuthenticationCallback;
import android.hardware.fingerprint.FingerprintManager.AuthenticationResult;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.minimal.MinimalUICoordinator.CompleteAndCloseObserver;
import org.chromium.chrome.browser.payments.minimal.MinimalUICoordinator.ConfirmObserver;
import org.chromium.chrome.browser.payments.minimal.MinimalUICoordinator.DismissObserver;
import org.chromium.chrome.browser.payments.minimal.MinimalUICoordinator.ErrorAndCloseObserver;
import org.chromium.chrome.browser.payments.minimal.MinimalUICoordinator.ReadyObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PaymentApp;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Payment minimal UI mediator, which is responsible for the interaction with the backend: the
 * coordinator, the fingerprint scanner, and the Payment Request API. It reacts to changes in the
 * backend and updates the model based on that, or receives events from the view and notifies the
 * backend.
 */
/* package */ class MinimalUIMediator implements BottomSheetObserver, OnClickListener {
    // 1 second delay to show processing message before showing the completion or error status.
    private static final int PROCESSING_DELAY_MS = 1000;

    // 500ms delay to show completion status before closing.
    private static final int COMPLETE_DELAY_MS = 500;

    // 2 second delay to show error status before closing or returning back to waiting state.
    private static final int ERROR_DELAY_MS = 2000;

    private final PaymentApp mApp;
    private final PropertyModel mModel;
    private final ConfirmObserver mConfirmObserver;
    private final DismissObserver mDismissObserver;
    private final Runnable mHider;
    private final FingerprintManager mFingerprintManager;
    private final CancellationSignal mCancellationSignal = new CancellationSignal();
    private final Handler mHandler = new Handler();
    private final boolean mIsFingerprintScanEnabled;
    private ReadyObserver mReadyObserver;
    private Runnable mPendingTask;
    private boolean mIsSheetOpened;
    private boolean mIsInProcessingState;

    /* package */ MinimalUIMediator(Context context, PaymentApp app, PropertyModel model,
            ReadyObserver readyObserver, ConfirmObserver confirmObserver,
            DismissObserver dismissObserver, Runnable hider) {
        mApp = app;
        mModel = model;
        mReadyObserver = readyObserver;
        mConfirmObserver = confirmObserver;
        mDismissObserver = dismissObserver;
        mHider = hider;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && new PackageManagerDelegate().hasSystemFeature(
                        PackageManager.FEATURE_FINGERPRINT)) {
            mFingerprintManager =
                    (FingerprintManager) context.getSystemService(Context.FINGERPRINT_SERVICE);
            mIsFingerprintScanEnabled = mFingerprintManager.isHardwareDetected()
                    && mFingerprintManager.hasEnrolledFingerprints();
            if (mIsFingerprintScanEnabled) {
                // MinimalUIMediator cannot implement AuthenticationCallback directly because the
                // API was added in Android M (API version 21).
                mFingerprintManager.authenticate(/*crypto=*/null, /*cancel=*/mCancellationSignal,
                        /*flags=*/0, /*callback=*/new AuthenticationCallback() {
                            @Override
                            public void onAuthenticationError(
                                    int errorCode, CharSequence errString) {
                                showErrorAndClose(() -> {
                                    mDismissObserver.onDismissed();
                                }, errString, /*errorMessageResourceId=*/null);
                            }

                            @Override
                            public void onAuthenticationHelp(
                                    int helpCode, CharSequence helpString) {
                                showErrorAndWait(helpString, /*errorMessageResourceId=*/null);
                            }

                            @Override
                            public void onAuthenticationSucceeded(AuthenticationResult result) {
                                showProcessingAndNotifyConfirmObserver();
                            }

                            @Override
                            public void onAuthenticationFailed() {
                                showErrorAndWait(/*errorMessage=*/null,
                                        R.string.payment_fingerprint_not_recognized);
                            }
                        }, /*handler=*/null);

                mModel.set(MinimalUIProperties.STATUS_ICON, R.drawable.ic_fingerprint_grey500_36dp);
                mModel.set(MinimalUIProperties.STATUS_ICON_TINT,
                        R.color.payment_minimal_ui_default_tint);
            }
        } else {
            mFingerprintManager = null;
            mIsFingerprintScanEnabled = false;
        }

        mModel.set(MinimalUIProperties.IS_SHOWING_PAY_BUTTON, !mIsFingerprintScanEnabled);
        mModel.set(MinimalUIProperties.STATUS_TEXT_RESOURCE,
                mIsFingerprintScanEnabled ? R.string.payment_touch_sensor_to_pay
                                          : R.string.payment_request_payment_method_section_name);
    }

    /* package */ void hide() {
        mHandler.removeCallbacksAndMessages(null);
        mCancellationSignal.cancel();
    }

    /* package */ void showCompleteAndClose(CompleteAndCloseObserver observer) {
        if (mIsInProcessingState) {
            mPendingTask = () -> {
                showCompleteAndClose(observer);
            };
            return;
        }

        mHandler.removeCallbacksAndMessages(null);
        mCancellationSignal.cancel();

        showEmphasizedStatus(R.string.payment_complete_message, null,
                R.drawable.ic_done_googblue_36dp, R.color.payment_minimal_ui_emphasis_tint);

        mHandler.postDelayed(() -> {
            mHider.run();
            observer.onCompletedAndClosed();
        }, COMPLETE_DELAY_MS);
    }

    /* package */ void showErrorAndClose(ErrorAndCloseObserver observer,
            @Nullable CharSequence errorMessage, @Nullable Integer errorMessageResourceId) {
        if (mIsInProcessingState) {
            mPendingTask = () -> {
                showErrorAndClose(observer, errorMessage, errorMessageResourceId);
            };
            return;
        }

        mHandler.removeCallbacksAndMessages(null);
        mCancellationSignal.cancel();

        showEmphasizedStatus(errorMessageResourceId, errorMessage, R.drawable.ic_error_googred_36dp,
                R.color.payment_minimal_ui_error_tint);

        mHandler.postDelayed(() -> {
            mHider.run();
            observer.onErroredAndClosed();
        }, ERROR_DELAY_MS);
    }

    private void showProcessingAndNotifyConfirmObserver() {
        mHandler.removeCallbacksAndMessages(null);
        mCancellationSignal.cancel();

        mModel.set(MinimalUIProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MinimalUIProperties.STATUS_TEXT, null);
        mModel.set(MinimalUIProperties.STATUS_TEXT_RESOURCE, R.string.payments_processing_message);
        if (mIsFingerprintScanEnabled) {
            mModel.set(MinimalUIProperties.STATUS_ICON, R.drawable.ic_fingerprint_grey500_36dp);
            mModel.set(
                    MinimalUIProperties.STATUS_ICON_TINT, R.color.payment_minimal_ui_emphasis_tint);
        } else {
            mModel.set(MinimalUIProperties.IS_SHOWING_PROCESSING_SPINNER, true);
            mModel.set(MinimalUIProperties.IS_SHOWING_LINE_ITEMS, false);
        }

        mIsInProcessingState = true;
        mHandler.postDelayed(() -> {
            mIsInProcessingState = false;
            if (mPendingTask != null) {
                mPendingTask.run();
                mPendingTask = null;
            }
        }, PROCESSING_DELAY_MS);

        mConfirmObserver.onConfirmed(mApp);
    }

    private void showErrorAndWait(
            @Nullable CharSequence errorMessage, @Nullable Integer errorMessageResourceId) {
        assert mIsFingerprintScanEnabled;

        mHandler.removeCallbacksAndMessages(null);

        showEmphasizedStatus(errorMessageResourceId, errorMessage, R.drawable.ic_error_googred_36dp,
                R.color.payment_minimal_ui_error_tint);

        mHandler.postDelayed(() -> {
            mModel.set(MinimalUIProperties.STATUS_TEXT, null);
            mModel.set(
                    MinimalUIProperties.STATUS_TEXT_RESOURCE, R.string.payment_touch_sensor_to_pay);
            mModel.set(MinimalUIProperties.IS_STATUS_EMPHASIZED, false);
            mModel.set(MinimalUIProperties.STATUS_ICON, R.drawable.ic_fingerprint_grey500_36dp);
            mModel.set(
                    MinimalUIProperties.STATUS_ICON_TINT, R.color.payment_minimal_ui_default_tint);
        }, ERROR_DELAY_MS);
    }

    private void showEmphasizedStatus(@Nullable Integer messageResourceId,
            @Nullable CharSequence message, @Nullable Integer iconResourceId,
            @Nullable Integer iconTint) {
        mModel.set(MinimalUIProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MinimalUIProperties.STATUS_TEXT, message);
        mModel.set(MinimalUIProperties.STATUS_TEXT_RESOURCE, messageResourceId);
        mModel.set(MinimalUIProperties.IS_STATUS_EMPHASIZED, true);
        mModel.set(MinimalUIProperties.STATUS_ICON, iconResourceId);
        mModel.set(MinimalUIProperties.STATUS_ICON_TINT, iconTint);
        mModel.set(MinimalUIProperties.IS_SHOWING_PROCESSING_SPINNER, false);

        if (!mIsFingerprintScanEnabled) {
            mModel.set(MinimalUIProperties.IS_SHOWING_LINE_ITEMS, false);
        }
    }

    // BottomSheetObserver:
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mIsSheetOpened = true;
        mModel.set(MinimalUIProperties.IS_PEEK_STATE_ENABLED, false);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {}

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        float oldAlpha = mModel.get(MinimalUIProperties.PAYMENT_APP_NAME_ALPHA);
        if (oldAlpha == 1f || !mIsSheetOpened) return;

        float newAlpha = heightFraction * 2f;
        if (oldAlpha >= newAlpha) return;

        mModel.set(MinimalUIProperties.PAYMENT_APP_NAME_ALPHA, newAlpha > 1f ? 1f : newAlpha);
    }

    @Override
    public void onSheetStateChanged(@SheetState int newState, int reason) {
        switch (newState) {
            case BottomSheetController.SheetState.HIDDEN:
                mHider.run();
                mDismissObserver.onDismissed();
                break;
            case BottomSheetController.SheetState.FULL:
                mModel.set(MinimalUIProperties.PAYMENT_APP_NAME_ALPHA, 1f);
                break;
        }
    }

    @Override
    public void onSheetFullyPeeked() {
        // Post to avoid destroying the native JourneyLogger before it has recoreded its events in
        // tests. JourneyLogger records events after MinimalUICoordinator.show() returns, which can
        // happen after onSheetFullyPeeked().
        mHandler.post(() -> {
            // onSheetFullyPeeked() can be invoked more than once, but mReadyObserver.onReady() is
            // expected to be called at most once.
            if (mReadyObserver == null) return;
            mReadyObserver.onReady();
            mReadyObserver = null;
        });
    }

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}

    // OnClickListener:
    @Override
    public void onClick(View v) {
        if (!mModel.get(MinimalUIProperties.IS_SHOWING_PAY_BUTTON)) return;
        showProcessingAndNotifyConfirmObserver();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    /* package */ void confirmForTest() {
        showProcessingAndNotifyConfirmObserver();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    /* package */ void dismissForTest() {
        onSheetStateChanged(BottomSheetController.SheetState.HIDDEN, StateChangeReason.NONE);
    }
}
