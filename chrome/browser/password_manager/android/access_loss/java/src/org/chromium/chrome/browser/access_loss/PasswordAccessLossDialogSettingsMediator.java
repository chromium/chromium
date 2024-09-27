// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.logDialogUserActionMetric;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the password access loss dialog meant to be shown in Chrome settings. It handles
 * interactions with the UI.
 */
class PasswordAccessLossDialogSettingsMediator implements ModalDialogProperties.Controller {
    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private final @PasswordAccessLossWarningType int mWarningType;
    private final Callback<Context> mLaunchGmsUpdate;
    private final Runnable mLaunchExportFlow;
    private final HelpUrlLauncher mHelpUrLauncher;

    public PasswordAccessLossDialogSettingsMediator(
            Activity activity,
            ModalDialogManager modalDialogManager,
            @PasswordAccessLossWarningType int warningType,
            Callback<Context> launchGmsUpdate,
            Runnable launchExportFlow,
            CustomTabIntentHelper customTabIntentHelper) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mWarningType = warningType;
        mLaunchGmsUpdate = launchGmsUpdate;
        mLaunchExportFlow = launchExportFlow;
        mHelpUrLauncher = new HelpUrlLauncher(customTabIntentHelper);
    }

    private void runPositiveButtonCallback() {
        switch (mWarningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                mLaunchExportFlow.run();
                break;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                mLaunchGmsUpdate.onResult(mActivity.getApplicationContext());
                break;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning";
                break;
        }
    }

    void onHelpButtonClicked() {
        logDialogUserActionMetric(mWarningType, PasswordAccessLossWarningUserAction.HELP_CENTER);
        switch (mWarningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                mHelpUrLauncher.showHelpArticle(
                        mActivity, HelpUrlLauncher.GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL);
                return;
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                assert false
                        : "The help button shouldn't be displayed for"
                                + " `NEW_GMS_CORE_MIGRATION_FAILED`.";
                return;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                mHelpUrLauncher.showHelpArticle(
                        mActivity,
                        HelpUrlLauncher.KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL);
                return;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning.";
                return;
        }

        assert false : "Value " + mWarningType + " of PasswordAccessLossWarningType is not known";
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            logDialogUserActionMetric(
                    mWarningType, PasswordAccessLossWarningUserAction.MAIN_ACTION);
            runPositiveButtonCallback();
        } else if (buttonType == ButtonType.NEGATIVE) {
            logDialogUserActionMetric(mWarningType, PasswordAccessLossWarningUserAction.DISMISS);
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
