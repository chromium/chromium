// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modules;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/**
 * UI informing the user about the status of installing a dynamic feature module. The UI consists of
 * toast for install start and success UI and an infobar in the failure case.
 */
public class ModuleInstallUi {
    private final Delegate mDelegate;
    private final int mModuleTitleStringId;
    private final FailureUiListener mFailureUiListener;
    private Toast mInstallStartToast;

    /**
     * Delegate holding methods getting the {@link WindowAndroid} and {@link Context} used to
     * display the UI. This is in case either changes over the lifetime of the class.
     */
    public interface Delegate {
        WindowAndroid getWindowAndroid();

        Context getContext();
    }

    /** Listener for when the user interacts with the install failure UI. */
    public interface FailureUiListener {
        /**
         * Called when the user makes a decision to handle the failure, either to retry installing
         * the module or to cancel installing the module by dismissing the UI.
         * @param retry Whether user decides to retry installing the module.
         */
        void onFailureUiResponse(boolean retry);
    }

    /*
     * Creates new UI.
     *
     * @param delegate Delegate providing the WindowAndroid and Context to display the UI.
     * @param moduleTitleStringId String resource ID of the module title
     * @param failureUiListener Listener for when the user interacts with the install failure UI.
     */
    public ModuleInstallUi(
            Delegate delegate, int moduleTitleStringId, FailureUiListener failureUiListener) {
        mDelegate = delegate;
        mModuleTitleStringId = moduleTitleStringId;
        mFailureUiListener = failureUiListener;
    }

    /** Show UI indicating the start of a module install. */
    public void showInstallStartUi() {
        Context context = mDelegate.getContext();
        if (context == null) return;
        mInstallStartToast =
                Toast.makeText(
                        context,
                        context.getString(
                                R.string.module_install_start_text,
                                context.getString(mModuleTitleStringId)),
                        Toast.LENGTH_SHORT);
        mInstallStartToast.show();
    }

    /** Show UI indicating the success of a module install. */
    public void showInstallSuccessUi() {
        if (mInstallStartToast != null) {
            mInstallStartToast.cancel();
            mInstallStartToast = null;
        }

        Context context = mDelegate.getContext();
        if (context == null) return;
        Toast.makeText(context, R.string.module_install_success_text, Toast.LENGTH_SHORT).show();
    }

    /**
     * Show UI indicating the failure of a module install. Upon interaction with the UI the
     * |failureUiListener| will be invoked.
     */
    public void showInstallFailureUi() {
        if (mInstallStartToast != null) {
            mInstallStartToast.cancel();
            mInstallStartToast = null;
        }

        Context context = mDelegate.getContext();
        WindowAndroid windowAndroid = mDelegate.getWindowAndroid();
        if (context == null || windowAndroid == null) {
            if (mFailureUiListener != null) mFailureUiListener.onFailureUiResponse(false);
            return;
        }

        String text =
                String.format(
                        context.getString(R.string.module_install_failure_text),
                        context.getResources().getString(mModuleTitleStringId));
        Snackbar snackbar =
                Snackbar.make(
                        text,
                        new SnackbarController() {
                            @Override
                            public void onAction(Object actionData) {
                                if (mFailureUiListener != null) {
                                    mFailureUiListener.onFailureUiResponse(true);
                                }
                            }

                            @Override
                            public void onDismissNoAction(Object actionData) {
                                if (mFailureUiListener != null) {
                                    mFailureUiListener.onFailureUiResponse(false);
                                }
                            }
                        },
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_MODULE_INSTALL_FAILURE);
        snackbar.setAction(context.getString(R.string.try_again), null);
        snackbar.setSingleLine(false);
        snackbar.setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS);
        SnackbarManager snackbarManager = SnackbarManagerProvider.from(windowAndroid);
        snackbarManager.showSnackbar(snackbar);
    }
}
