// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog shown to user to confirm deleting a saved credit card. */
public class AutofillDeleteCreditCardConfirmationDialog {

    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private final Callback<Integer> mResultHandler;

    public AutofillDeleteCreditCardConfirmationDialog(
            ModalDialogManager modalDialogManager,
            Context context,
            Callback<Integer> resultHandler) {
        mModalDialogManager = modalDialogManager;
        mContext = context;
        mResultHandler = resultHandler;
    }

    /** Displays an AutofillDeleteCreditCardConfirmationDialog. */
    public void show() {
        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(mModalDialogManager, mResultHandler);

        final int titleResId = R.string.autofill_credit_card_delete_confirmation_title;
        final int descResId = R.string.autofill_credit_card_delete_confirmation_description;
        PropertyModel dialog =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, mContext.getString(titleResId))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                mContext.getString(descResId))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(R.string.delete))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(R.string.cancel))
                        .build();
        mModalDialogManager.showDialog(dialog, ModalDialogManager.ModalDialogType.APP);
    }
}
