// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.logDialogShownMetric;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.DETAILS;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.HELP_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.HELP_BUTTON_VISIBILITY;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.TITLE;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Shows the warning to the user explaining why they are not able to access Google Password Manager.
 * This warning is displayed when user tries to access Google Password Manager in 3 cases: 1) They
 * don't have GMS Core installed. 2) They have an outdated GMS Core. 3) The local passwords
 * migration to GMS Core failed. The user is then proposed to either update GMS Core (if it's a
 * valid option) or to run the passwords export flow.
 */
public class PasswordAccessLossDialogSettingsCoordinator {
    private Context mContext;
    private ModalDialogManager mModalDialogManager;
    private PasswordAccessLossDialogSettingsMediator mMediator;

    public void showPasswordAccessLossDialog(
            Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @PasswordAccessLossWarningType int warningType,
            Callback<Context> launchGmsUpdate,
            Runnable launchExportFlow,
            CustomTabIntentHelper customTabIntentHelper) {
        assert warningType != PasswordAccessLossWarningType.NONE
                : "Only show the access loss dialog if there is a reason to warn about.";
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mMediator =
                new PasswordAccessLossDialogSettingsMediator(
                        ContextUtils.activityFromContext(context),
                        modalDialogManager,
                        warningType,
                        launchGmsUpdate,
                        launchExportFlow,
                        customTabIntentHelper);
        View dialogCustomView = createAndBindDialogCustomView(warningType);
        mModalDialogManager.showDialog(
                createDialogModel(context, warningType, dialogCustomView),
                ModalDialogManager.ModalDialogType.APP);
        logDialogShownMetric(warningType);
    }

    private View createAndBindDialogCustomView(@PasswordAccessLossWarningType int warningType) {
        View dialogCustomView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.access_loss_dialog_settings_view, null);
        Resources resources = mContext.getResources();
        PropertyModel model =
                new PropertyModel.Builder(PasswordAccessLossDialogSettingsProperties.ALL_KEYS)
                        .with(TITLE, resources.getString(getTitle(warningType)))
                        .with(DETAILS, resources.getString(getDescription(warningType)))
                        .with(HELP_BUTTON_VISIBILITY, getHelpButtonVisible(warningType))
                        .with(HELP_BUTTON_CALLBACK, mMediator::onHelpButtonClicked)
                        .build();
        PropertyModelChangeProcessor.create(
                model, dialogCustomView, PasswordAccessLossDialogSettingsViewBinder::bind);
        return dialogCustomView;
    }

    private PropertyModel createDialogModel(
            Context context, @PasswordAccessLossWarningType int warningType, View customView) {
        Resources resources = context.getResources();
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mMediator)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
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

    private boolean getHelpButtonVisible(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
            case PasswordAccessLossWarningType.NO_UPM:
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return true;
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return false;
            case PasswordAccessLossWarningType.NONE:
                assert false
                        : "Illegal value `PasswordAccessLossWarningType.NONE` when trying to show"
                                + " password access loss warning";
                return false;
        }
        assert false : "Value of PasswordAccessLossWarningType is not known";
        return false;
    }
}
