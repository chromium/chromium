// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

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
    @IntDef({DialogState.NOT_SHOWN, DialogState.SHOWN, DialogState.DISMISSED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface DialogState {
        int NOT_SHOWN = 0;
        int SHOWN = 1;
        int DISMISSED = 2;
    }

    private static final int SHOW_WEB_SIGNIN_LOADING_DIALOG_DELAY_MS = 1000;

    private final RunnableTimer mShowDialogTimer = new RunnableTimer();
    private @Nullable Tab mTab;
    private @Nullable GURL mContinueUrl;
    private @Nullable GURL mInitialTabURL;
    private @DialogState int mDialogState = DialogState.NOT_SHOWN;
    private @Nullable WebSigninBridge mWebSigninBridge;
    private @Nullable PropertyModel mModel;
    private @Nullable ModalDialogManager mDialogManager;

    /**
     * If refresh tokens and cookies are successfully minted for the account associated with the
     * selected email, a redirect is triggered to the continueUrl in the given tab. A dialog is
     * displayed if the minting process takes too long.
     */
    public void initializeWebSigninAndRedirect(
            Tab tab, String email, GURL continueUrl, GURL initialTabURL) {
        mTab = tab;
        mContinueUrl = continueUrl;
        mInitialTabURL = initialTabURL;

        // It's possible that this is invoked before a previous WebSigninBridge has responded so
        // destroy any previous bridges.
        destroy();

        mWebSigninBridge =
                new WebSigninBridge.Factory()
                        .createWithEmail(tab.getProfile(), email, this::onSigninResult);

        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_WEB_SIGNIN_LOADING_DIALOG)) {
            mShowDialogTimer.startTimer(SHOW_WEB_SIGNIN_LOADING_DIALOG_DELAY_MS, this::showDialog);
        }
    }

    /**
     * If refresh tokens and cookies are successfully minted for the account associated with
     * selectedAccountId, a redirect is triggered to the continueUrl in the given tab. A dialog is
     * displayed if the minting process takes too long.
     */
    public void initializeWebSigninAndRedirect(
            Tab tab, CoreAccountId accountId, GURL continueUrl, GURL initialTabURL) {
        mTab = tab;
        mContinueUrl = continueUrl;
        mInitialTabURL = initialTabURL;

        // It's possible that this is invoked before a previous WebSigninBridge has responded so
        // destroy any previous bridges.
        destroy();

        mWebSigninBridge =
                new WebSigninBridge.Factory()
                        .createWithCoreAccountId(tab.getProfile(), accountId, this::onSigninResult);

        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_WEB_SIGNIN_LOADING_DIALOG)) {
            mShowDialogTimer.startTimer(SHOW_WEB_SIGNIN_LOADING_DIALOG_DELAY_MS, this::showDialog);
        }
    }

    /**
     * Releases native resources used by this class and cancels the timer to show a loading dialog.
     */
    public void destroy() {
        mShowDialogTimer.cancelTimer();
        if (mWebSigninBridge != null) {
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }

        if (mDialogState == DialogState.SHOWN) {
            mDialogState = DialogState.DISMISSED;
            if (mModel != null && mDialogManager != null) {
                mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTION_ON_CONTENT);
                mModel = null;
            }
        }
    }

    public void setTabForTesting(Tab tab) {
        mTab = tab;
    }

    public @Nullable PropertyModel getDialogModelForTesting() {
        return mModel;
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

        View customView =
                LayoutInflater.from(mTab.getContext())
                        .inflate(R.layout.web_signin_loading_dialog, null);
        customView.findViewById(R.id.cancel_button).setOnClickListener(v -> destroy());

        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mDialogManager.showDialog(mModel, ModalDialogType.TAB);
    }

    private Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (mDialogState == DialogState.SHOWN) {
                    destroy();
                }
            }
        };
    }

    private void onSigninResult(@WebSigninTrackerResult int result) {
        assert mTab != null;
        assert mContinueUrl != null;
        assert mInitialTabURL != null;

        destroy();

        switch (result) {
            case WebSigninTrackerResult.SUCCESS:
                if (!mTab.isDestroyed() && mTab.getUrl().equals(mInitialTabURL)) {
                    mTab.loadUrl(new LoadUrlParams(mContinueUrl));
                }
                break;
            // TODO(crbug.com/456445865): Handle cases where WebSigninTracker returns an error.
            case WebSigninTrackerResult.AUTH_ERROR:
                break;
            case WebSigninTrackerResult.OTHER_ERROR:
                break;
        }
    }
}
