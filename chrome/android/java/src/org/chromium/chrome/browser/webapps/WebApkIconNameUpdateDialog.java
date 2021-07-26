// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The dialog that warns the user that a WebApk is about to be updated, which will result in a
 * short name, long name and/or icon change.
 */
public class WebApkIconNameUpdateDialog implements ModalDialogProperties.Controller {
    private static Boolean sActionToTakeInTests;

    private static final String TAG = "IconNameUpdateDlg";

    // The modal dialog manager to use.
    private ModalDialogManager mModalDialogManager;

    // The property model for the dialog.
    private PropertyModel mDialogModel;

    // The short name of the app before update.
    private String mOldAppShortName;

    // The package name for this app.
    private String mPackageName;

    // The callback to run when the user has made a decision.
    private Callback<Integer> mDialogResultCallback;

    public WebApkIconNameUpdateDialog() {}

    /**
     * Shows the dialog.
     * @param manager The {@ModalDialogManager} to use.
     * @param packageName The package name for this app.
     * @param iconChanging Whether an icon change has been detected.
     * @param shortNameChanging Whether a short name change has been detected.
     * @param nameChanging Whether a name change has been detected.
     * @param oldAppShortName The short name of the currently installed app.
     * @param newAppShortName The proposed short name for the updated app.
     * @param oldAppName The name of the currently installed app.
     * @param newAppName The proposed name for the updated app.
     * @param oldIcon The icon of the currently installed app.
     * @param newIcon The proposed new icon for the updated app.
     * @param oldIconAdaptive Whether the current icon is adaptive.
     * @param newIconAdaptive Whether the updated icon is adaptive.
     * @param callback The callback to use to communicate the results.
     */
    public void show(ModalDialogManager manager, String packageName, boolean iconChanging,
            boolean shortNameChanging, boolean nameChanging, String oldAppShortName,
            String newAppShortName, String oldAppName, String newAppName, Bitmap currentAppIcon,
            Bitmap updatedAppIcon, boolean oldIconAdaptive, boolean newIconAdaptive,
            Callback<Integer> callback) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        mOldAppShortName = oldAppShortName;
        mPackageName = packageName;
        mDialogResultCallback = callback;

        int titleId = 0;
        if (iconChanging && (shortNameChanging || nameChanging)) {
            titleId = R.string.webapp_update_dialog_title_name_and_icon;
        } else {
            titleId = iconChanging ? R.string.webapp_update_dialog_title_icon
                                   : R.string.webapp_update_dialog_title_name;
        }

        WebApkIconNameUpdateCustomView dialogCustomView =
                (WebApkIconNameUpdateCustomView) LayoutInflaterUtils.inflate(
                        context, R.layout.webapk_icon_name_update_dialog, null);
        // Always show the icon, because the dialog looks weird if only WebappInfo#shortname or
        // WebappInfo#name is changing.
        dialogCustomView.configureIcons(
                currentAppIcon, updatedAppIcon, oldIconAdaptive, newIconAdaptive);
        if (nameChanging) {
            dialogCustomView.configureNames(oldAppName, newAppName);
        }

        // Show the webapp short name in the scenario that neither the webapp short name nor the web
        // app name has changed. This is to make it clearer which app is changing its identity in
        // this scenario.
        if (shortNameChanging || !nameChanging) {
            dialogCustomView.configureShortNames(oldAppShortName, newAppShortName);
        }

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, resources, titleId)
                        .with(ModalDialogProperties.MESSAGE, resources,
                                R.string.webapp_update_explanation)
                        .with(ModalDialogProperties.CUSTOM_VIEW, dialogCustomView)
                        .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, true)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.webapp_update_negative_button)
                        .with(ModalDialogProperties.TITLE_SCROLLABLE, true)
                        .build();

        mModalDialogManager = manager;
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                break;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                WebApkUpdateReportAbuseDialog reportAbuseDialog = new WebApkUpdateReportAbuseDialog(
                        mModalDialogManager, mPackageName, mOldAppShortName,
                        /* showAbuseCheckbox= */ false, this::onUninstall);
                reportAbuseDialog.show();
                break;
            default:
                Log.i(TAG, "Unexpected button pressed in dialog: " + buttonType);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogResultCallback.onResult(dismissalCause);
    }

    /**
     * Dismisses us. Called when the child dialog on top of this dialog requests dismissal, because
     * the user has confirmed the uninstall of the app.
     */
    private void onUninstall() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }
}
