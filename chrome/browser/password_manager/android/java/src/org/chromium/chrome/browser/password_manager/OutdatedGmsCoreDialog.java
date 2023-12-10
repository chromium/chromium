// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog that informs user that the GMS Core needs to be updated and confirms whether the user
 * agrees to proceed with the update.
 */
class OutdatedGmsCoreDialog {
    static final String DISMISSAL_REASON_HISTOGRAM =
            "PasswordManager.OutdatedGMSDialogDismissalReason";
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private final Callback<Boolean> mResultHandler;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        GmsUpdateDialogDismissalReason.OTHER,
        GmsUpdateDialogDismissalReason.ACCEPTED,
        GmsUpdateDialogDismissalReason.REJECTED,
        GmsUpdateDialogDismissalReason.COUNT
    })
    @interface GmsUpdateDialogDismissalReason {
        int OTHER = 0;
        int ACCEPTED = 1;
        int REJECTED = 2;

        int COUNT = 3;
    }

    /**
     * Constructor for {@link OutdatedGmsCoreDialog}.
     *
     * @param modalDialogManager The {@link ModalDialogManager} which is going to display the
     *         dialog.
     * @param context The context for accessing resources.
     * @param resultHandler Handler to be called on whether the user have chosen to update or not.
     */
    OutdatedGmsCoreDialog(
            ModalDialogManager modalDialogManager,
            Context context,
            Callback<Boolean> resultHandler) {
        mModalDialogManager = modalDialogManager;
        mContext = context;
        mResultHandler = resultHandler;
    }

    /** Shows the dialog. */
    void show() {
        SimpleModalDialogController modalDialogController =
                new SimpleModalDialogController(mModalDialogManager, this::onDismissedWithReason);

        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                mContext.getResources()
                                        .getString(
                                                R.string
                                                        .password_manager_outdated_gms_dialog_description))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getResources()
                                        .getString(
                                                R.string
                                                        .password_manager_outdated_gms_positive_button))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getResources()
                                        .getString(
                                                R.string
                                                        .password_manager_outdated_gms_negative_button))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        mModalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private void onDismissedWithReason(int dismissalReason) {
        // The dismissal reason should be recorded before the result handler is called, because
        // the latter might open a new activity, which can cause the current one to be destroyed.
        RecordHistogram.recordEnumeratedHistogram(
                DISMISSAL_REASON_HISTOGRAM,
                getDismissalReasonForMetrics(dismissalReason),
                GmsUpdateDialogDismissalReason.COUNT);
        mResultHandler.onResult(dismissalReason == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    private @GmsUpdateDialogDismissalReason int getDismissalReasonForMetrics(
            int dialogDismissalReason) {
        if (dialogDismissalReason == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            return GmsUpdateDialogDismissalReason.ACCEPTED;
        }
        if (dialogDismissalReason == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED
                || dialogDismissalReason == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
            return GmsUpdateDialogDismissalReason.REJECTED;
        }
        return GmsUpdateDialogDismissalReason.OTHER;
    }
}
