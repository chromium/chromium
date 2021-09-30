// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

/**
 * A simple UNDO snackbar with a delay.
 */
class AssistantSnackbar {
    interface Callback {
        /**
         * Called once the snackbar is gone, after the delay has passed or after the user clicked
         * undo.
         *
         * @param undo {@code true} if the user clicked undo
         */
        void onDismiss(boolean undo);
    }

    /** Shows the snackbar and reports the result to {@code callback}. */
    static SnackbarController show(Context context, SnackbarManager snackbarManager, int delayMs,
            String message, String undoString, Callback callback) {
        SnackbarController controller = new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                callback.onDismiss(/* undo= */ true);
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                callback.onDismiss(/* undo= */ false);
            }
        };
        Snackbar snackBar = Snackbar.make(message, controller, Snackbar.TYPE_ACTION,
                                            Snackbar.UMA_AUTOFILL_ASSISTANT_STOP_UNDO)
                                    .setAction(!TextUtils.isEmpty(undoString)
                                                    ? undoString
                                                    : context.getString(R.string.undo),
                                            /* actionData= */ null);
        snackBar.setSingleLine(false);
        snackBar.setDuration(delayMs);
        snackbarManager.showSnackbar(snackBar);
        return controller;
    }
}
