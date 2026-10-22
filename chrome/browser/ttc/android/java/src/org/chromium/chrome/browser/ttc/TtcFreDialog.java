// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.content.res.Resources;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The TTC first run dialog, shown before the first session starts. Accepting it completes the FRE.
 *
 * <p>TODO(crbug.com/575529047): this is a placeholder; the real dialog hasn't been finalized yet.
 *
 * <p>TODO(crbug.com/575965025): the mocks anchor the dialog to the top of the content area,
 * overlapping the toolbar; no Android modal dialog presenter supports that yet, so it's centered.
 */
@NullMarked
class TtcFreDialog {
    private TtcFreDialog() {}

    /**
     * Shows the dialog as a window-modal dialog, so it stays up across tab switches until the user
     * answers it.
     *
     * @param modalDialogManager The window's dialog manager.
     * @param resources Used to resolve the dialog's strings.
     * @param onDismissed Run once the dialog goes away, with whether the user accepted. Declining,
     *     dismissing, or {@link ModalDialogManager#dismissDialog dismissing it programmatically}
     *     all report false.
     * @return The dialog's model, for dismissing it via {@link ModalDialogManager#dismissDialog}.
     */
    static PropertyModel show(
            ModalDialogManager modalDialogManager,
            Resources resources,
            Callback<Boolean> onDismissed) {
        ModalDialogProperties.Controller controller =
                new SimpleModalDialogController(
                        modalDialogManager,
                        dismissalCause ->
                                onDismissed.onResult(
                                        dismissalCause
                                                == DialogDismissalCause.POSITIVE_BUTTON_CLICKED));
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, resources, R.string.ttc_fre_dialog_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(R.string.ttc_fre_dialog_body))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.ttc_fre_dialog_allow)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();
        modalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
        return model;
    }
}
