// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningHelper.GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningHelper.KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
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
    private final CustomTabIntentHelper mCustomTabIntentHelper;

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
        mCustomTabIntentHelper = customTabIntentHelper;
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
        switch (mWarningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                openUrlInCct(GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL);
                return;
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                assert false
                        : "The help button shouldn't be displayed for"
                                + " `NEW_GMS_CORE_MIGRATION_FAILED`.";
                return;
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                openUrlInCct(KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL);
                return;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning.";
                return;
        }

        assert false : "Value " + mWarningType + " of PasswordAccessLossWarningType is not known";
    }

    // TODO(crbug.com/365755043): Use HelpAndFeedbackLauncher once support for p-link is added
    // or make this reusable by other components.
    void openUrlInCct(String url) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                mCustomTabIntentHelper.createCustomTabActivityIntent(
                        mActivity, customTabIntent.intent);
        intent.setPackage(mActivity.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mActivity.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(mActivity, intent);
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
