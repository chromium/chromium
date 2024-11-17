// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.CallbackUtils;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Helps to show a confirmation. */
public class ConfirmationDialogHelper {
    private final Context mContext;
    private ModalDialogManager mModalDialogManager;
    private PropertyModel mDialogModel;
    private Runnable mConfirmedCallback;
    private Runnable mDeclinedCallback;

    public ConfirmationDialogHelper(Context context) {
        mContext = context;
        mModalDialogManager =
                new ModalDialogManager(new AppModalPresenter(context), ModalDialogType.APP);
    }

    /** Hides the dialog. */
    public void dismiss() {
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.UNKNOWN);
        }
        mDialogModel = null;
    }

    /** Returns the resources associated with the context used to launch the dialog. */
    public Resources getResources() {
        return mContext.getResources();
    }

    /**
     * Shows an dialog to confirm the deletion.
     *
     * @param title A {@link String} used as title.
     * @param message A {@link String} used message body.
     * @param confirmButtonTextId A string ID for positive button label.
     * @param confirmedCallback A callback to run when the dialog is accepted.
     * @param declinedCallback A callback to run when the dialog is declined.
     */
    public void showConfirmation(
            String title, String message, int confirmButtonTextId, Runnable confirmedCallback) {
        showConfirmation(
                title,
                message,
                confirmButtonTextId,
                confirmedCallback,
                CallbackUtils.emptyRunnable());
    }

    /**
     * Shows an dialog to confirm the deletion.
     *
     * @param title A {@link String} used as title.
     * @param message A {@link String} used message body.
     * @param confirmButtonTextId A string ID for positive button label.
     * @param confirmedCallback A callback to run when the dialog is accepted.
     * @param declinedCallback A callback to run when the dialog is declined.
     */
    public void showConfirmation(
            String title,
            String message,
            int confirmButtonTextId,
            Runnable confirmedCallback,
            Runnable declinedCallback) {
        assert title != null;
        assert message != null;
        assert confirmedCallback != null;

        mConfirmedCallback = confirmedCallback;
        mDeclinedCallback = declinedCallback;

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        mModalDialogManager, this::onDismiss))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, message)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(confirmButtonTextId))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(R.string.cancel))
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogType.APP);
    }

    private void onDismiss(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                mConfirmedCallback.run();
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                mDeclinedCallback.run();
                break;
            default:
                // No explicit user decision.
                break;
        }
        mDialogModel = null;
    }

    void setModalDialogManagerForTesting(ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;
    }
}
