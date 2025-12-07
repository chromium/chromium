// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Helps to show a confirmation. */
@NullMarked
public class ConfirmationDialogHelper {
    private final Context mContext;
    private ModalDialogManager mModalDialogManager;
    private @Nullable PropertyModel mDialogModel;
    private @Nullable Runnable mConfirmedCallback;
    private @Nullable Runnable mDeclinedCallback;

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
     * @param message A {@link CharSequence} used message body.
     * @param confirmButtonText A {@link String} for confirmation button label.
     * @param confirmedCallback A callback to run when the dialog is accepted.
     * @param declinedCallback A callback to run when the dialog is declined.
     */
    public void showConfirmation(
            String title,
            CharSequence message,
            String confirmButtonText,
            Runnable confirmedCallback) {
        showConfirmation(
                title,
                message,
                confirmButtonText,
                confirmedCallback,
                CallbackUtils.emptyRunnable());
    }

    /**
     * Shows an dialog to confirm the deletion.
     *
     * @param title A {@link String} used as title.
     * @param message A {@link CharSequence} used message body.
     * @param confirmButtonText A {@link String} for confirmation button label.
     * @param confirmedCallback A callback to run when the dialog is accepted.
     * @param declinedCallback A callback to run when the dialog is declined.
     */
    public void showConfirmation(
            String title,
            CharSequence message,
            String confirmButtonText,
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
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPHS,
                                new ArrayList<>(List.of(message)))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, confirmButtonText)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(R.string.cancel))
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogType.APP);
    }

    private void onDismiss(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                assumeNonNull(mConfirmedCallback);
                mConfirmedCallback.run();
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                assumeNonNull(mDeclinedCallback);
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
