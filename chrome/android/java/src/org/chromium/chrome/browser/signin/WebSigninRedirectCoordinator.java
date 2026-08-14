// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.RunnableTimer;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinator that waits for cookies to be built and displays a dialog if it takes more than {@link
 * SHOW_WEB_SIGNIN_LOADING_DIALOG_DELAY_MS}.
 */
@NullMarked
public class WebSigninRedirectCoordinator {
    // LINT.IfChange(DialogState)
    @IntDef({DialogState.NOT_SHOWN, DialogState.SHOWN, DialogState.DISMISSED})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface DialogState {
        int NOT_SHOWN = 0;
        int SHOWN = 1;
        int DISMISSED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:WebSigninLoadingDialogStatus)

    // LINT.IfChange(WaitOutcome)
    @IntDef({
        WaitOutcome.COMPLETED_WITHOUT_DIALOG,
        WaitOutcome.COMPLETED_WITH_DIALOG,
        WaitOutcome.ABORTED_WITHOUT_DIALOG,
        WaitOutcome.ABORTED_WITH_DIALOG_CANCEL_BUTTON,
        WaitOutcome.ABORTED_WITH_DIALOG_TOUCH_OUTSIDE,
        WaitOutcome.ABORTED_WITH_DIALOG_OTHER,
        WaitOutcome.AUTH_ERROR_WITHOUT_DIALOG,
        WaitOutcome.AUTH_ERROR_WITH_DIALOG,
        WaitOutcome.OTHER_ERROR_WITHOUT_DIALOG,
        WaitOutcome.OTHER_ERROR_WITH_DIALOG
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface WaitOutcome {
        int COMPLETED_WITHOUT_DIALOG = 0;
        int COMPLETED_WITH_DIALOG = 1;
        int ABORTED_WITHOUT_DIALOG = 2;
        int ABORTED_WITH_DIALOG_CANCEL_BUTTON = 3;
        int ABORTED_WITH_DIALOG_TOUCH_OUTSIDE = 4;
        int ABORTED_WITH_DIALOG_OTHER = 5;
        int AUTH_ERROR_WITHOUT_DIALOG = 6;
        int AUTH_ERROR_WITH_DIALOG = 7;
        int OTHER_ERROR_WITHOUT_DIALOG = 8;
        int OTHER_ERROR_WITH_DIALOG = 9;
        int NUM_ENTRIES = 10;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:WebSigninWaitOutcome)

    private static final int SHOW_WEB_SIGNIN_LOADING_DIALOG_DELAY_MS = 1000;
    private static final int MIN_DIALOG_VISIBLE_DURATION_MS = 500;
    private RunnableTimer mShowDialogTimer = new RunnableTimer();
    private RunnableTimer mMinDialogVisibleTimer = new RunnableTimer();
    private @Nullable Tab mTab;
    private @Nullable GURL mContinueUrl;
    private @Nullable GURL mInitialTabURL;
    private @DialogState int mDialogState = DialogState.NOT_SHOWN;
    private @Nullable WebSigninBridge mWebSigninBridge;
    private @Nullable PropertyModel mModel;
    private @Nullable ModalDialogManager mDialogManager;
    private boolean mMinShowTimePassed;
    private boolean mIsSigninResultReceived;
    private @Nullable Integer mDeferredSigninResult;
    private long mInitializeStartTime;
    private boolean mIsWaitOutcomeRecorded;
    private @WaitOutcome int mWaitOutcomeReason = WaitOutcome.ABORTED_WITHOUT_DIALOG;

    /**
     * If refresh tokens and cookies are successfully minted for the account associated with the
     * selected email, a redirect is triggered to the continueUrl in the given tab. A dialog is
     * displayed if the minting process takes too long.
     */
    public void initializeWebSigninAndRedirect(
            Tab tab, String email, GURL continueUrl, GURL initialTabURL) {
        // If the dialog is already shown, don't destroy it if initialize is called again.
        if (mDialogState == DialogState.SHOWN) {
            return;
        }

        // It's possible that this is invoked before a previous WebSigninBridge has responded so
        // destroy any previous bridges.
        destroy();

        mIsWaitOutcomeRecorded = false;
        mWaitOutcomeReason = WaitOutcome.ABORTED_WITHOUT_DIALOG;
        mInitializeStartTime = SystemClock.elapsedRealtime();
        mTab = tab;
        mContinueUrl = continueUrl;
        mInitialTabURL = initialTabURL;

        mWebSigninBridge =
                new WebSigninBridge.Factory()
                        .createWithEmail(tab.getProfile(), email, this::onSigninResult);

        startTimerOrForceShowDialog();
    }

    /**
     * If refresh tokens and cookies are successfully minted for the account associated with
     * selectedAccountId, a redirect is triggered to the continueUrl in the given tab. A dialog is
     * displayed if the minting process takes too long.
     */
    public void initializeWebSigninAndRedirect(
            Tab tab, CoreAccountId accountId, GURL continueUrl, GURL initialTabURL) {
        // If the dialog is already shown, don't destroy it if initialize is called again.
        if (mDialogState == DialogState.SHOWN) {
            return;
        }

        // It's possible that this is invoked before a previous WebSigninBridge has responded so
        // destroy any previous bridges.
        destroy();

        mIsWaitOutcomeRecorded = false;
        mWaitOutcomeReason = WaitOutcome.ABORTED_WITHOUT_DIALOG;
        mInitializeStartTime = SystemClock.elapsedRealtime();
        mTab = tab;
        mContinueUrl = continueUrl;
        mInitialTabURL = initialTabURL;

        mWebSigninBridge =
                new WebSigninBridge.Factory()
                        .createWithCoreAccountId(tab.getProfile(), accountId, this::onSigninResult);

        startTimerOrForceShowDialog();
    }

    /**
     * Releases native resources used by this class and cancels the timer to show a loading dialog.
     */
    public void destroy() {
        mShowDialogTimer.cancelTimer();
        mMinDialogVisibleTimer.cancelTimer();
        mDeferredSigninResult = null;

        if (mWebSigninBridge != null) {
            recordWaitOutcomeAborted();
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }

        if (mDialogState == DialogState.SHOWN) {
            if (!mIsSigninResultReceived) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Signin.ProcessMirrorHeaders.LoadingDialog.Status",
                        DialogState.DISMISSED,
                        DialogState.NUM_ENTRIES);
            }
            // Reset dialog state to NOT_SHOWN in case the same coordinator is reused.
            mDialogState = DialogState.NOT_SHOWN;
            if (mModel != null && mDialogManager != null) {
                mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTION_ON_CONTENT);
                mModel = null;
            }
        }
        // Reset these outside the if block to ensure state is cleared even if dialog was not shown,
        // in case the coordinator is reused.
        mIsSigninResultReceived = false;
        mMinShowTimePassed = false;
    }

    public void setTabForTesting(Tab tab) {
        mTab = tab;
    }

    public @Nullable PropertyModel getDialogModelForTesting() {
        return mModel;
    }

    public void setShowDialogTimerForTesting(RunnableTimer timer) {
        mShowDialogTimer = timer;
    }

    public void setMinDialogVisibleTimerForTesting(RunnableTimer timer) {
        mMinDialogVisibleTimer = timer;
    }

    @VisibleForTesting
    public void showDialog() {
        if (mTab == null) {
            destroy();
            return;
        }

        if (mDialogState != DialogState.NOT_SHOWN || mTab.isDestroyed()) {
            destroy();
            return;
        }

        WindowAndroid windowAndroid = mTab.getWindowAndroid();
        if (windowAndroid == null) {
            destroy();
            return;
        }
        mDialogManager = windowAndroid.getModalDialogManager();
        if (mDialogManager == null) {
            destroy();
            return;
        }

        mDialogState = DialogState.SHOWN;
        mWaitOutcomeReason = WaitOutcome.ABORTED_WITH_DIALOG_OTHER;
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.ProcessMirrorHeaders.LoadingDialog.Status",
                DialogState.SHOWN,
                DialogState.NUM_ENTRIES);
        mMinShowTimePassed = false;
        mMinDialogVisibleTimer.startTimer(
                MIN_DIALOG_VISIBLE_DURATION_MS, this::onMinShowTimePassed);

        View customView =
                LayoutInflater.from(mTab.getContext())
                        .inflate(R.layout.web_signin_loading_dialog, null);
        customView
                .findViewById(R.id.cancel_button)
                .setOnClickListener(
                        v -> {
                            mWaitOutcomeReason = WaitOutcome.ABORTED_WITH_DIALOG_CANCEL_BUTTON;
                            destroy();
                        });

        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.CONTENT_DESCRIPTION,
                                mTab.getContext()
                                        .getString(R.string.signin_web_signin_loading_dialog_title))
                        .build();

        mDialogManager.showDialog(mModel, ModalDialogType.TAB);
    }

    private Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.TOUCH_OUTSIDE) {
                    mWaitOutcomeReason = WaitOutcome.ABORTED_WITH_DIALOG_TOUCH_OUTSIDE;
                } else {
                    mWaitOutcomeReason = WaitOutcome.ABORTED_WITH_DIALOG_OTHER;
                }

                destroy();
            }
        };
    }

    private void onMinShowTimePassed() {
        mMinShowTimePassed = true;
        if (mDeferredSigninResult != null) {
            onSigninResult(mDeferredSigninResult);
        }
    }

    private void onSigninResult(@WebSigninTrackerResult int result) {
        if (mWebSigninBridge == null) {
            // mWebSigninBridge has already been destroyed so return early.
            return;
        }
        assert mTab != null;
        assert mContinueUrl != null;
        assert mInitialTabURL != null;

        if (!mIsSigninResultReceived) {
            mIsSigninResultReceived = true;
            if (mDialogState == DialogState.NOT_SHOWN) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Signin.ProcessMirrorHeaders.LoadingDialog.Status",
                        DialogState.NOT_SHOWN,
                        DialogState.NUM_ENTRIES);
            }

            RecordHistogram.recordTimesHistogram(
                    "Signin.ProcessMirrorHeaders.LoadingDuration"
                            + getWebSigninTrackerResultString(result),
                    SystemClock.elapsedRealtime() - mInitializeStartTime);
        }

        if (mDialogState == DialogState.SHOWN && !mMinShowTimePassed) {
            // The dialog has to be displayed for at least {@link MINIMUM_SHOW_TIME_MS}.
            mDeferredSigninResult = result;
            return;
        }

        if (SigninFeatureMap.isEnabled(SigninFeatures.FORCE_SHOW_WEB_SIGNIN_LOADING_DIALOG)) {
            return;
        }

        boolean willRedirect = false;
        switch (result) {
            case WebSigninTrackerResult.SUCCESS:
                if (!mTab.isDestroyed()
                        && mTab.getWebContents() != null
                        && mTab.getWebContents().getLastCommittedUrl().equals(mInitialTabURL)) {
                    willRedirect = true;
                }
                break;
            case WebSigninTrackerResult.AUTH_ERROR:
                mWaitOutcomeReason =
                        mDialogState == DialogState.NOT_SHOWN
                                ? WaitOutcome.AUTH_ERROR_WITHOUT_DIALOG
                                : WaitOutcome.AUTH_ERROR_WITH_DIALOG;
                break;
            case WebSigninTrackerResult.OTHER_ERROR:
                mWaitOutcomeReason =
                        mDialogState == DialogState.NOT_SHOWN
                                ? WaitOutcome.OTHER_ERROR_WITHOUT_DIALOG
                                : WaitOutcome.OTHER_ERROR_WITH_DIALOG;
                break;
        }

        if (!willRedirect) {
            destroy();
            return;
        }

        recordWaitOutcomeCompleted();
        destroy();
        mTab.loadUrl(new LoadUrlParams(mContinueUrl));
    }

    private void startTimerOrForceShowDialog() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.FORCE_SHOW_WEB_SIGNIN_LOADING_DIALOG)) {
            showDialog();
        } else if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_WEB_SIGNIN_LOADING_DIALOG)) {
            mShowDialogTimer.startTimer(SHOW_WEB_SIGNIN_LOADING_DIALOG_DELAY_MS, this::showDialog);
        }
    }

    private static String getWebSigninTrackerResultString(int result) {
        String resultSuffix = "";
        switch (result) {
            case WebSigninTrackerResult.SUCCESS:
                resultSuffix = ".Success";
                break;
            case WebSigninTrackerResult.AUTH_ERROR:
                resultSuffix = ".AuthError";
                break;
            case WebSigninTrackerResult.OTHER_ERROR:
                resultSuffix = ".OtherError";
                break;
        }
        return resultSuffix;
    }

    private void recordWaitOutcomeCompleted() {
        if (mIsWaitOutcomeRecorded) return;
        mIsWaitOutcomeRecorded = true;
        int outcome =
                (mDialogState == DialogState.NOT_SHOWN)
                        ? WaitOutcome.COMPLETED_WITHOUT_DIALOG
                        : WaitOutcome.COMPLETED_WITH_DIALOG;
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.ProcessMirrorHeaders.WaitOutcome", outcome, WaitOutcome.NUM_ENTRIES);
    }

    private void recordWaitOutcomeAborted() {
        if (mIsWaitOutcomeRecorded) return;
        mIsWaitOutcomeRecorded = true;
        int outcome = mWaitOutcomeReason;
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.ProcessMirrorHeaders.WaitOutcome", outcome, WaitOutcome.NUM_ENTRIES);
        boolean isErrorOutcome =
                outcome == WaitOutcome.AUTH_ERROR_WITH_DIALOG
                        || outcome == WaitOutcome.AUTH_ERROR_WITHOUT_DIALOG
                        || outcome == WaitOutcome.OTHER_ERROR_WITH_DIALOG
                        || outcome == WaitOutcome.OTHER_ERROR_WITHOUT_DIALOG;
        if (mInitializeStartTime != 0 && !mIsSigninResultReceived && !isErrorOutcome) {
            RecordHistogram.recordTimesHistogram(
                    "Signin.ProcessMirrorHeaders.LoadingDuration.Aborted",
                    SystemClock.elapsedRealtime() - mInitializeStartTime);
        }
    }
}
