// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.autofill_assistant.AssistantSnackbar;
import org.chromium.components.autofill_assistant.AssistantSnackbarFactory;

/**
 * Implementation of {@link AssistantSnackbarFactory} for Chrome.
 */
public class AssistantSnackbarFactoryChrome implements AssistantSnackbarFactory {
    private Context mContext;
    private SnackbarManager mSnackbarManager;

    public AssistantSnackbarFactoryChrome(Context context, SnackbarManager snackbarManager) {
        mContext = context;
        mSnackbarManager = snackbarManager;
    }

    @Override
    public AssistantSnackbar createSnackbar(
            int delayMs, String message, String undoString, Callback<Boolean> callback) {
        SnackbarController controller = new SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                callback.onResult(/* undo= */ true);
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                callback.onResult(/* undo= */ false);
            }
        };

        String actionText =
                TextUtils.isEmpty(undoString) ? mContext.getString(R.string.undo) : undoString;

        Snackbar snackBar = Snackbar.make(message, controller, Snackbar.TYPE_ACTION,
                                            Snackbar.UMA_AUTOFILL_ASSISTANT_STOP_UNDO)
                                    .setAction(actionText,
                                            /* actionData= */ null);
        snackBar.setSingleLine(false);
        snackBar.setDuration(delayMs);

        return new AssistantSnackbar() {
            @Override
            public void show() {
                mSnackbarManager.showSnackbar(snackBar);
            }

            @Override
            public void dismiss() {
                mSnackbarManager.dismissSnackbars(controller);
            }
        };
    }
}
