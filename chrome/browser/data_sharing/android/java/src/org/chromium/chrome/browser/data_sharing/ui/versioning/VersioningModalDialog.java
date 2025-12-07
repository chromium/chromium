// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import static org.chromium.ui.modaldialog.ModalDialogProperties.ALL_KEYS;
import static org.chromium.ui.modaldialog.ModalDialogProperties.BUTTON_STYLES;
import static org.chromium.ui.modaldialog.ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE;
import static org.chromium.ui.modaldialog.ModalDialogProperties.CONTROLLER;
import static org.chromium.ui.modaldialog.ModalDialogProperties.MESSAGE_PARAGRAPH_1;
import static org.chromium.ui.modaldialog.ModalDialogProperties.NEGATIVE_BUTTON_TEXT;
import static org.chromium.ui.modaldialog.ModalDialogProperties.POSITIVE_BUTTON_TEXT;
import static org.chromium.ui.modaldialog.ModalDialogProperties.TITLE;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Constructs and shows a dialog suggesting the user update their version of Chrome. Positive action
 * will take the user ot the Play Store.
 */
@NullMarked
public class VersioningModalDialog {
    /**
     * Shows a dialog prompting the user to update Chrome both negative and positive buttons.
     *
     * @param context Used to load resources and launch intents.
     * @param modalDialogManager Used to show as a dialog.
     */
    public static void show(Context context, ModalDialogManager modalDialogManager) {
        new VersioningModalDialog(context, modalDialogManager, /* exitRunnable= */ null).show();
    }

    /**
     * Shows a dialog prompting the user to update Chrome both negative and positive buttons.
     *
     * @param context Used to load resources and launch intents.
     * @param modalDialogManager Used to show as a dialog.
     * @param message Custom dialog message to show.
     * @param exitRunnable The runnable to run when exiting the modal dialog.
     * @return The property model of the dialog shown.
     */
    public static PropertyModel showWithCustomMessage(
            Context context,
            ModalDialogManager modalDialogManager,
            String message,
            Runnable exitRunnable) {
        return new VersioningModalDialog(context, modalDialogManager, exitRunnable).show(message);
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final @Nullable Runnable mExitRunnable;

    /**
     * @param context Used to load resources and launch intents.
     * @param modalDialogManager Used to show as a dialog.
     * @param exitRunnable The runnable to run when exiting the modal dialog.
     */
    public VersioningModalDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            @Nullable Runnable exitRunnable) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mExitRunnable = exitRunnable;
    }

    /** Shows a dialog prompting the user to update Chrome both negative and positive buttons. */
    public void show() {
        mModalDialogManager.showDialog(getModelBuilder().build(), ModalDialogType.APP);
    }

    private PropertyModel show(String message) {
        PropertyModel model = getModelBuilder().with(MESSAGE_PARAGRAPH_1, message).build();
        mModalDialogManager.showDialog(model, ModalDialogType.APP);
        return model;
    }

    private PropertyModel.Builder getModelBuilder() {
        Controller controller =
                new SimpleModalDialogController(mModalDialogManager, this::onDismiss);
        String title =
                mContext.getString(R.string.collaboration_chrome_out_of_date_error_dialog_header);
        String message =
                mContext.getString(
                        R.string.collaboration_chrome_out_of_date_error_dialog_continue_body);
        String positiveButton =
                mContext.getString(
                        R.string.collaboration_chrome_out_of_date_error_dialog_update_button);
        String negativeButton =
                mContext.getString(
                        R.string.collaboration_chrome_out_of_date_error_dialog_not_now_button);
        return new PropertyModel.Builder(ALL_KEYS)
                .with(CONTROLLER, controller)
                .with(TITLE, title)
                .with(MESSAGE_PARAGRAPH_1, message)
                .with(POSITIVE_BUTTON_TEXT, positiveButton)
                .with(NEGATIVE_BUTTON_TEXT, negativeButton)
                .with(CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(BUTTON_STYLES, ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);
    }

    private void onDismiss(@DialogDismissalCause Integer dismissalCause) {
        assert dismissalCause != null;
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            String chromeAppId = mContext.getPackageName();
            String uriString = ContentUrlConstants.PLAY_STORE_URL_PREFIX + chromeAppId;
            Uri uri = Uri.parse(uriString);
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            mContext.startActivity(intent);
        }

        if (mExitRunnable != null) {
            mExitRunnable.run();
        }
    }
}
