// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/**
 * Shows a warning to the user explaining why the password manager is not available. For users who
 * can update GMS Core to regain access, it displays an update button. For all other users it only
 * displays information.
 */
@NullMarked
public class PasswordManagerUnavailableDialogCoordinator {
    private Context mContext;
    private @Nullable Callback<Context> mLaunchGmsUpdateCallback;
    private ModalDialogManager mModalDialogManager;
    private PasswordManagerUnavailableDialogMediator mMediator;

    @Initializer
    public void showDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            @Nullable Callback<Context> launchGmsUpdate) {
        mContext = context;
        mLaunchGmsUpdateCallback = launchGmsUpdate;
        mModalDialogManager = modalDialogManager;
        boolean isUpdateDialog = (launchGmsUpdate != null);
        mMediator =
                new PasswordManagerUnavailableDialogMediator(
                        modalDialogManager, this::launchGmsUpdateFlow, isUpdateDialog);

        mModalDialogManager.showDialog(
                createDialogModel(isUpdateDialog), ModalDialogManager.ModalDialogType.APP);
    }

    private void launchGmsUpdateFlow() {
        assumeNonNull(mLaunchGmsUpdateCallback);
        mLaunchGmsUpdateCallback.onResult(mContext);
    }

    private PropertyModel createDialogModel(boolean isUpdateDialog) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mMediator)
                .with(ModalDialogProperties.TITLE, getTitle(isUpdateDialog))
                .with(
                        ModalDialogProperties.MESSAGE_PARAGRAPHS,
                        getMessageParagraphs(isUpdateDialog))
                .with(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        getPositiveButtonText(isUpdateDialog))
                .with(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        getNegativeButtonText(isUpdateDialog))
                .with(ModalDialogProperties.BUTTON_STYLES, getButtonStyles(isUpdateDialog))
                .build();
    }

    private String getTitle(boolean isUpdateDialog) {
        return isUpdateDialog
                ? mContext.getString(R.string.access_loss_update_gms_title)
                : mContext.getString(R.string.pwm_disabled_no_gms_dialog_title);
    }

    private ArrayList<CharSequence> getMessageParagraphs(boolean isUpdateDialog) {
        ArrayList<CharSequence> messages = new ArrayList<>();
        if (isUpdateDialog) {
            messages.add(mContext.getString(R.string.pwm_disabled_update_dialog_description));
        } else {
            messages.add(
                    mContext.getString(R.string.pwm_disabled_no_gms_dialog_description_paragraph1));
            messages.add(
                    mContext.getString(R.string.pwm_disabled_no_gms_dialog_description_paragraph2));
        }
        return messages;
    }

    private String getPositiveButtonText(boolean isUpdateDialog) {
        return isUpdateDialog
                ? mContext.getString(R.string.pwd_access_loss_warning_update_gms_core_button_text)
                : mContext.getString(R.string.pwm_disabled_no_gms_dialog_button_text);
    }

    private @Nullable String getNegativeButtonText(boolean isUpdateDialog) {
        return isUpdateDialog
                ? mContext.getString(R.string.pwm_disabled_update_dialog_cancel)
                : null;
    }

    private int getButtonStyles(boolean isUpdateDialog) {
        return isUpdateDialog
                ? ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE
                : ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE;
    }
}
