// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator for the Read Aloud privacy opt-in modal dialog shown before sharing page content and
 * URLs with Google.
 */
@NullMarked
public class ReadAloudOptInDialogCoordinator {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Callback<Boolean> mOnOptInDecision;
    private @Nullable PropertyModel mDialogModel;

    public ReadAloudOptInDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            Callback<Boolean> onOptInDecision) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOnOptInDecision = onOptInDecision;
    }

    /** Shows the opt-in modal dialog. */
    public void show() {
        if (mDialogModel != null) {
            return;
        }

        View customView =
                LayoutInflater.from(mContext).inflate(R.layout.readaloud_opt_in_dialog, null);

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(
                            PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            mModalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                            mModalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        }
                    }

                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        mDialogModel = null;
                        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                            mOnOptInDecision.onResult(true);
                        } else if (isUserDecline(dismissalCause)) {
                            mOnOptInDecision.onResult(false);
                        }
                    }
                };

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(R.string.readaloud_opt_in_accept_button))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(R.string.cancel))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /** Dismisses the dialog if currently showing. */
    public void dismiss() {
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.ACTIVITY_DESTROYED);
        }
    }

    /**
     * Returns whether the dismissal was an explicit user decline. Tapping outside is disabled, so
     * the back-related causes can only come from the system back button or gesture, which is
     * treated the same as tapping Cancel. Programmatic dismissals (e.g. ACTIVITY_DESTROYED) are not
     * a decision and are ignored.
     */
    private static boolean isUserDecline(@DialogDismissalCause int dismissalCause) {
        return dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED
                || dismissalCause == DialogDismissalCause.NAVIGATE_BACK
                || dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;
    }
}
