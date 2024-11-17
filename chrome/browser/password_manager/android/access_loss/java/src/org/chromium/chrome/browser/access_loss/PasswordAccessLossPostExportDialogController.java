// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.DETAILS;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.HELP_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.HELP_BUTTON_VISIBILITY;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.TITLE;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Shows the warning to the user explaining why they are not able to access Google Password Manager.
 * This warning is displayed for the users who don't have GMS Core installed and don't have
 * passwords in the profile store (either they never had them or passwords were removed after export
 * flow).
 */
public class PasswordAccessLossPostExportDialogController
        implements ModalDialogProperties.Controller {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final HelpUrlLauncher mHelpUrLauncher;

    public PasswordAccessLossPostExportDialogController(
            Context context,
            @NonNull ModalDialogManager modalDialogManager,
            CustomTabIntentHelper customTabIntentHelper) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mHelpUrLauncher = new HelpUrlLauncher(customTabIntentHelper);
    }

    public void showPostExportDialog() {
        View dialogCustomView = createAndBindDialogCustomView();
        mModalDialogManager.showDialog(
                createDialogModel(dialogCustomView), ModalDialogManager.ModalDialogType.APP);
    }

    private View createAndBindDialogCustomView() {
        View dialogCustomView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.access_loss_dialog_settings_view, null);
        Resources resources = mContext.getResources();
        PropertyModel model =
                new PropertyModel.Builder(PasswordAccessLossDialogSettingsProperties.ALL_KEYS)
                        .with(
                                TITLE,
                                resources.getString(R.string.access_loss_no_gms_no_passwords_title))
                        .with(
                                DETAILS,
                                resources.getString(R.string.access_loss_no_gms_no_passwords_desc))
                        .with(HELP_BUTTON_VISIBILITY, true)
                        .with(
                                HELP_BUTTON_CALLBACK,
                                () -> {
                                    Activity activity = ContextUtils.activityFromContext(mContext);
                                    if (activity == null) return;
                                    mHelpUrLauncher.showHelpArticle(
                                            activity,
                                            HelpUrlLauncher
                                                    .GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL);
                                })
                        .build();
        PropertyModelChangeProcessor.create(
                model, dialogCustomView, PasswordAccessLossDialogSettingsViewBinder::bind);
        return dialogCustomView;
    }

    private PropertyModel createDialogModel(View customView) {
        Resources resources = mContext.getResources();
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources, R.string.close)
                .with(
                        ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                .build();
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        mModalDialogManager.dismissDialog(
                model,
                buttonType == ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {}
}
