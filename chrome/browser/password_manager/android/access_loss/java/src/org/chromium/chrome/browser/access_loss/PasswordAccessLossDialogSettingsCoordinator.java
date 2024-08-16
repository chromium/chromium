// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Shows the warning to the user explaining why they are not able to access Google Password Manager.
 */
public class PasswordAccessLossDialogSettingsCoordinator
        implements ModalDialogProperties.Controller {
    private Context mContext;
    private ModalDialogManager mModalDialogManager;
    private @PasswordAccessLossWarningType int mWarningType;
    private Callback<Context> mLaunchGmsUpdate;
    private Callback<Context> mLaunchExportFlow;

    public void showPasswordAccessLossDialog(
            Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @PasswordAccessLossWarningType int warningType,
            Callback<Context> launchGmsUpdate,
            Callback<Context> launchExportFlow) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mWarningType = warningType;
        mLaunchGmsUpdate = launchGmsUpdate;
        mLaunchExportFlow = launchExportFlow;
        mModalDialogManager.showDialog(
                createDialogModel(context, warningType), ModalDialogManager.ModalDialogType.APP);
    }

    private PropertyModel createDialogModel(
            Context context, @PasswordAccessLossWarningType int warningType) {
        Resources resources = context.getResources();
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(ModalDialogProperties.TITLE, resources, getTitle(warningType))
                .with(
                        ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                        resources.getString(getDescription(warningType)))
                .with(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        resources,
                        getPositiveButtonText(warningType))
                .with(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        resources,
                        getNegativeButtonText(warningType))
                .with(
                        ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                .build();
    }

    private int getTitle(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return R.string.access_loss_no_gms_title;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return R.string.access_loss_update_gms_title;
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return R.string.access_loss_fix_problem_title;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning";
                return 0;
        }
        assert false : "Value of PasswordAccessLossWarningType is not known";
        return 0;
    }

    private int getDescription(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return R.string.access_loss_no_gms_desc;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return R.string.access_loss_update_gms_desc;
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return R.string.access_loss_fix_problem_desc;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning";
                return 0;
        }
        assert false : "Value of PasswordAccessLossWarningType is not known";
        return 0;
    }

    private int getPositiveButtonText(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return R.string.access_loss_no_gms_positive_button_text;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return R.string.password_manager_outdated_gms_positive_button;
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return R.string.access_loss_fix_problem_positive_button_text;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning";
                return 0;
        }
        assert false : "Value of PasswordAccessLossWarningType is not known";
        return 0;
    }

    private int getNegativeButtonText(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return R.string.close;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return R.string.password_manager_outdated_gms_negative_button;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning";
                return 0;
        }
        assert false : "Value of PasswordAccessLossWarningType is not known";
        return 0;
    }

    private void runPositiveButtonCallback() {
        switch (mWarningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                mLaunchExportFlow.onResult(mContext);
                break;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                mLaunchGmsUpdate.onResult(mContext);
                break;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning";
                break;
        }
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            runPositiveButtonCallback();
        }
        mModalDialogManager.dismissDialog(
                model,
                buttonType == ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {}
}
