// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

public abstract class SafetyHubBaseFragment extends ChromeBaseSettingsFragment {
    private SnackbarManager mSnackbarManager;

    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    protected void showSnackbar(
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        if (mSnackbarManager != null) {
            showSnackbar(mSnackbarManager, text, identifier, controller, actionData);
        }
    }

    protected void showSnackbarOnLastFocusedActivity(
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof SnackbarManager.SnackbarManageable) {
            SnackbarManager snackbarManager =
                    ((SnackbarManager.SnackbarManageable) activity).getSnackbarManager();
            if (snackbarManager != null) {
                showSnackbar(snackbarManager, text, identifier, controller, actionData);
            }
        }
    }

    private void showSnackbar(
            SnackbarManager snackbarManager,
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        var snackbar = Snackbar.make(text, controller, Snackbar.TYPE_ACTION, identifier);
        snackbar.setAction(getString(R.string.undo), actionData);
        snackbar.setSingleLine(false);

        snackbarManager.showSnackbar(snackbar);
    }
}
