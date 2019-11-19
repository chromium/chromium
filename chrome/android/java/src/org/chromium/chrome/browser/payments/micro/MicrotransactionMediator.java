// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.micro;

import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.fingerprint.FingerprintManager;
import android.hardware.fingerprint.FingerprintManager.AuthenticationCallback;
import android.hardware.fingerprint.FingerprintManager.AuthenticationResult;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.PackageManagerDelegate;
import org.chromium.chrome.browser.payments.PaymentInstrument;
import org.chromium.chrome.browser.payments.micro.MicrotransactionCoordinator.CompleteAndCloseObserver;
import org.chromium.chrome.browser.payments.micro.MicrotransactionCoordinator.ConfirmObserver;
import org.chromium.chrome.browser.payments.micro.MicrotransactionCoordinator.DismissObserver;
import org.chromium.chrome.browser.payments.micro.MicrotransactionCoordinator.ErrorAndCloseObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Microtransaction mediator, which is responsible for the interaction with the backend: the
 * coordinator, the fingerprint scanner, and the Payment Request API. It reacts to changes in the
 * backend and updates the model based on that, or receives events from the view and notifies the
 * backend.
 */
/* package */ class MicrotransactionMediator implements BottomSheetObserver, OnClickListener {
    // 1 second delay to show processing message before showing the completion or error status.
    private static final int PROCESSING_DELAY_MS = 1000;

    // 500ms delay to show completion status before closing.
    private static final int COMPLETE_DELAY_MS = 500;

    // 2 second delay to show error status before closing or returning back to waiting state.
    private static final int ERROR_DELAY_MS = 2000;

    private final PaymentInstrument mInstrument;
    private final PropertyModel mModel;
    private final ConfirmObserver mConfirmObserver;
    private final DismissObserver mDismissObserver;
    private final Runnable mHider;
    private final FingerprintManager mFingerprintManager;
    private final CancellationSignal mCancellationSignal = new CancellationSignal();
    private final Handler mHandler = new Handler();
    private final boolean mIsFingerprintScanEnabled;
    private Runnable mPendingTask;
    private boolean mIsSheetOpened;
    private boolean mIsInProcessingState;

    /* package */ MicrotransactionMediator(Context context, PaymentInstrument instrument,
            PropertyModel model, ConfirmObserver confirmObserver, DismissObserver dismissObserver,
            Runnable hider) {
        mInstrument = instrument;
        mModel = model;
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
                // MicrotransactionMediator cannot implement AuthenticationCallback directly because
                // the API was added in Android M (API version 21).
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
                                showProcessing();
                                mConfirmObserver.onConfirmed(mInstrument);
                            }

                            @Override
                            public void onAuthenticationFailed() {
                                showErrorAndWait(/*errorMessage=*/null,
                                        R.string.payment_fingerprint_not_recognized);
                            }
                        }, /*handler=*/null);

                mModel.set(MicrotransactionProperties.STATUS_ICON,
                        R.drawable.ic_fingerprint_grey500_36dp);
                mModel.set(MicrotransactionProperties.STATUS_ICON_TINT,
                        R.color.microtransaction_default_tint);
            }
        } else {
            mFingerprintManager = null;
            mIsFingerprintScanEnabled = false;
        }

        mModel.set(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, !mIsFingerprintScanEnabled);
        mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE,
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
                R.drawable.ic_done_googblue_36dp, R.color.microtransaction_emphasis_tint);

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
                R.color.microtransaction_error_tint);

        mHandler.postDelayed(() -> {
            mHider.run();
            observer.onErroredAndClosed();
        }, ERROR_DELAY_MS);
    }

    private void showProcessing() {
        mHandler.removeCallbacksAndMessages(null);
        mCancellationSignal.cancel();

        mModel.set(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MicrotransactionProperties.STATUS_TEXT, null);
        mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE,
                R.string.payments_processing_message);
        if (mIsFingerprintScanEnabled) {
            mModel.set(
                    MicrotransactionProperties.STATUS_ICON, R.drawable.ic_fingerprint_grey500_36dp);
            mModel.set(MicrotransactionProperties.STATUS_ICON_TINT,
                    R.color.microtransaction_emphasis_tint);
        } else {
            mModel.set(MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER, true);
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        }

        mIsInProcessingState = true;
        mHandler.postDelayed(() -> {
            mIsInProcessingState = false;
            if (mPendingTask != null) {
                mPendingTask.run();
                mPendingTask = null;
            }
        }, PROCESSING_DELAY_MS);
    }

    private void showErrorAndWait(
            @Nullable CharSequence errorMessage, @Nullable Integer errorMessageResourceId) {
        assert mIsFingerprintScanEnabled;

        mHandler.removeCallbacksAndMessages(null);

        showEmphasizedStatus(errorMessageResourceId, errorMessage, R.drawable.ic_error_googred_36dp,
                R.color.microtransaction_error_tint);

        mHandler.postDelayed(() -> {
            mModel.set(MicrotransactionProperties.STATUS_TEXT, null);
            mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE,
                    R.string.payment_touch_sensor_to_pay);
            mModel.set(MicrotransactionProperties.IS_STATUS_EMPHASIZED, false);
            mModel.set(
                    MicrotransactionProperties.STATUS_ICON, R.drawable.ic_fingerprint_grey500_36dp);
            mModel.set(MicrotransactionProperties.STATUS_ICON_TINT,
                    R.color.microtransaction_default_tint);
        }, ERROR_DELAY_MS);
    }

    private void showEmphasizedStatus(@Nullable Integer messageResourceId,
            @Nullable CharSequence message, @Nullable Integer iconResourceId,
            @Nullable Integer iconTint) {
        mModel.set(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MicrotransactionProperties.STATUS_TEXT, message);
        mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE, messageResourceId);
        mModel.set(MicrotransactionProperties.IS_STATUS_EMPHASIZED, true);
        mModel.set(MicrotransactionProperties.STATUS_ICON, iconResourceId);
        mModel.set(MicrotransactionProperties.STATUS_ICON_TINT, iconTint);
        mModel.set(MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER, false);

        if (!mIsFingerprintScanEnabled) {
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        }
    }

    // BottomSheetObserver:
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mIsSheetOpened = true;
        mModel.set(MicrotransactionProperties.IS_PEEK_STATE_ENABLED, false);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {}

    @Override
    public void onLoadUrl(String url) {}

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        float oldAlpha = mModel.get(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA);
        if (oldAlpha == 1f || !mIsSheetOpened) return;

        float newAlpha = heightFraction * 2f;
        if (oldAlpha >= newAlpha) return;

        mModel.set(
                MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, newAlpha > 1f ? 1f : newAlpha);
    }

    @Override
    public void onSheetStateChanged(@SheetState int newState) {
        switch (newState) {
            case BottomSheetController.SheetState.HIDDEN:
                mHider.run();
                mDismissObserver.onDismissed();
                break;
            case BottomSheetController.SheetState.FULL:
                mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
                break;
        }
    }

    @Override
    public void onSheetFullyPeeked() {}

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}

    // OnClickListener:
    @Override
    public void onClick(View v) {
        if (!mModel.get(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON)) return;
        showProcessing();
        mConfirmObserver.onConfirmed(mInstrument);
    }
}
