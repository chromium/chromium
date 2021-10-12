// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modules;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.ui.widget.Toast;

/**
 * UI informing the user about the status of installing a dynamic feature module. The UI consists of
 * toast for install start and success UI and an infobar in the failure case.
 */
public class ModuleInstallUi {
    private final Tab mTab;
    private final int mModuleTitleStringId;
    private final FailureUiListener mFailureUiListener;
    private Toast mInstallStartToast;

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
     * @param tab Tab in whose context to show the UI.
     * @param moduleTitleStringId String resource ID of the module title
     * @param failureUiListener Listener for when the user interacts with the install failure UI.
     */
    public ModuleInstallUi(Tab tab, int moduleTitleStringId, FailureUiListener failureUiListener) {
        mTab = tab;
        mModuleTitleStringId = moduleTitleStringId;
        mFailureUiListener = failureUiListener;
    }

    /** Show UI indicating the start of a module install. */
    public void showInstallStartUi() {
        Context context = TabUtils.getActivity(mTab);
        if (context == null) {
            // Tab is detached. Don't show UI.
            return;
        }
        mInstallStartToast = Toast.makeText(context,
                context.getString(R.string.module_install_start_text,
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

        Context context = TabUtils.getActivity(mTab);
        if (context == null) {
            // Tab is detached. Don't show UI.
            return;
        }
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

        Context context = TabUtils.getActivity(mTab);
        if (context == null) {
            // Tab is detached. Cancel.
            if (mFailureUiListener != null) mFailureUiListener.onFailureUiResponse(false);
            return;
        }

        SimpleConfirmInfoBarBuilder.Listener listener = new SimpleConfirmInfoBarBuilder.Listener() {
            @Override
            public void onInfoBarDismissed() {
                if (mFailureUiListener != null) mFailureUiListener.onFailureUiResponse(false);
            }

            @Override
            public boolean onInfoBarButtonClicked(boolean isPrimary) {
                if (mFailureUiListener != null) {
                    mFailureUiListener.onFailureUiResponse(isPrimary);
                }
                return false;
            }

            @Override
            public boolean onInfoBarLinkClicked() {
                return false;
            }
        };

        String text = String.format(context.getString(R.string.module_install_failure_text),
                context.getResources().getString(mModuleTitleStringId));
        SimpleConfirmInfoBarBuilder.create(mTab.getWebContents(), listener,
                InfoBarIdentifier.MODULE_INSTALL_FAILURE_INFOBAR_ANDROID, context,
                R.drawable.ic_error_outline_googblue_24dp, text,
                context.getString(R.string.try_again), context.getString(R.string.cancel),
                /* linkText = */ null, /* autoExpire = */ true);
    }
}
