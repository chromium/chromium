// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Shows the snackbar for Privacy Sandbox settings, allowing the user to quickly navigate there. */
public class PrivacySandboxSnackbarController implements SnackbarManager.SnackbarController {
    private Context mContext;
    private SnackbarManager mSnackbarManager;

    /** Creates an instance of the controller given a SnackbarManager. */
    public PrivacySandboxSnackbarController(Context context, SnackbarManager manager) {
        ThreadUtils.assertOnUiThread();
        assert manager != null;
        mContext = context;
        mSnackbarManager = manager;
    }

    /** Displays a snackbar, showing the user an option to go to Privacy Sandbox settings. */
    public void showSnackbar() {
        RecordUserAction.record("Settings.PrivacySandbox.Block3PCookies");
        mSnackbarManager.dismissSnackbars(this);
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                mContext.getString(R.string.privacy_sandbox_snackbar_message),
                                this,
                                Snackbar.TYPE_PERSISTENT,
                                Snackbar.UMA_PRIVACY_SANDBOX_PAGE_OPEN)
                        .setAction(mContext.getString(R.string.more), null)
                        .setSingleLine(false));
    }

    /** Dismisses the snackbar, if it is active. */
    public void dismissSnackbar() {
        mSnackbarManager.dismissSnackbars(this);
    }

    // Implement SnackbarController.
    @Override
    public void onAction(Object actionData) {
        PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                mContext, PrivacySandboxReferrer.COOKIES_SNACKBAR);
    }

    @Override
    public void onDismissNoAction(Object actionData) {}
}
