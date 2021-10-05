// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.fre;

import android.content.Context;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.base.ThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator handles the update and interaction of the FRE sign-in screen.
 */
@MainThread
public class SigninFirstRunCoordinator {
    /** Listener for signin fist run MVC. */
    public interface Listener {
        /** Notifies when the user clicked the "add account" button. */
        void addAccount();

        /** Notifies when the user accepts the terms of service. */
        void acceptTermsOfService();
    }

    private final SigninFirstRunMediator mMediator;

    /**
     * Constructs a coordinator instance.
     *
     * @param context is used to create the UI.
     * @param view is the FRE bottom group view including the selected account, the continue/
     *        dismiss buttons and other view components that change according to different state.
     * @param modalDialogManager is used to open dialogs like account picker dialog and uma dialog.
     * @param listener is invoked to interact with classes outside the module.
     */
    public SigninFirstRunCoordinator(
            Context context, View view, ModalDialogManager modalDialogManager, Listener listener) {
        mMediator = new SigninFirstRunMediator(context, modalDialogManager, listener);
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), view, SigninFirstRunViewBinder::bind);
    }

    /**
     * Releases the resources used by the coordinator.
     */
    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Notifies that native is loaded, and if policies are available, that they are also available.
     * @param hasPolicies whether policies are found on device.
     */
    public void onNativeAndPolicyLoaded(boolean hasPolicies) {
        ThreadUtils.assertOnUiThread();
        mMediator.onNativeAndPolicyLoaded(hasPolicies);
    }

    public void onAccountSelected(String accountName) {
        mMediator.onAccountSelected(accountName);
    }
}