// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogHandler;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogParams;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DialogDismissType;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Helper class to show a confirmation dialog when an action would stop an active Actor task. */
@NullMarked
public class ActorNavigationConfirmationDialog {

    /**
     * Shows the confirmation dialog.
     *
     * @param context The context to use for resources.
     * @param modalDialogManager The ModalDialogManager to display the dialog.
     * @param onConfirm Callback invoked with true if the user confirms stopping the task, false if
     *     they cancel.
     */
    public static void show(
            Context context, ModalDialogManager modalDialogManager, Callback<Boolean> onConfirm) {
        ConfirmationDialogHandler onDialogInteracted =
                (dismissHandler, buttonClickResult, resultStopShowing) -> {
                    boolean leave = buttonClickResult == ButtonClickResult.POSITIVE;
                    if (onConfirm != null) {
                        onConfirm.onResult(leave);
                    }
                    return DialogDismissType.DISMISS_IMMEDIATELY;
                };
        ActionConfirmationDialog dialog = new ActionConfirmationDialog(context, modalDialogManager);
        dialog.show(
                new ConfirmationDialogParams.Builder(context)
                        .withTitle(R.string.actor_leave_site_dialog_title)
                        .withDescription(R.string.actor_leave_site_dialog_description)
                        .withPositiveButton(R.string.actor_leave_site_dialog_leave_site)
                        .withNegativeButton(R.string.cancel)
                        .withSupportStopShowing(false)
                        .build(),
                onDialogInteracted);
    }
}
