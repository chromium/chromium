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


/** Dialog shown to user to confirm deleting a saved payment method. */
public class AutofillDeletePaymentMethodConfirmationDialog {

    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private final Callback<Integer> mResultHandler;
    private final int mTitleResId;

    public AutofillDeletePaymentMethodConfirmationDialog(
            ModalDialogManager modalDialogManager,
            Context context,
            Callback<Integer> resultHandler,
            int titleResId) {
        mModalDialogManager = modalDialogManager;
        mContext = context;
        mResultHandler = resultHandler;
        mTitleResId = titleResId;
    }

    /** Displays an AutofillDeletePaymentMethodConfirmationDialog. */
    public void show() {
        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(mModalDialogManager, mResultHandler);

        final int descResId = R.string.autofill_payment_method_delete_confirmation_description;
        PropertyModel dialog =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, mContext.getString(mTitleResId))
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
