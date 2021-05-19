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

    // The callback to run when the user has made a decision.
    private Callback<Integer> mDialogResultCallback;

    public WebApkIconNameUpdateDialog() {}

    /**
     * Shows the dialog.
     * @param manager The {@ModalDialogManager} to use.
     * @param iconChanging Whether an icon change has been detected.
     * @param shortNameChanging Whether a short name change has been detected.
     * @param nameChanging Whether a name change has been detected.
     * @param oldAppShortName The short name of the currently installed app.
     * @param newAppShortName The proposed short name for the updated app.
     * @param oldAppName The name of the currently installed app.
     * @param newAppName The proposed name for the updated app.
     * @param oldIcon The icon of the currently installed app.
     * @param newIcon The proposed new icon for the updated app.
     * @param oldIconAdaptive Wheter the current icon is adaptive.
     * @param newIconAdaptive Wheter the updated icon is adaptive.
     * @param callback The callback to use to communicate the results.
     */
    public void show(ModalDialogManager manager, boolean iconChanging, boolean shortNameChanging,
            boolean nameChanging, String oldAppShortName, String newAppShortName, String oldAppName,
            String newAppName, Bitmap currentAppIcon, Bitmap updatedAppIcon,
            boolean oldIconAdaptive, boolean newIconAdaptive, Callback<Integer> callback) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        mDialogResultCallback = callback;

        int titleId = 0;
        int explanationId = 0;
        if (iconChanging && (shortNameChanging || nameChanging)) {
            titleId = R.string.webapk_update_dialog_title_name_and_icon;
            explanationId = R.string.webapk_update_explanation_name_and_icon;
        } else {
            titleId = iconChanging ? R.string.webapk_update_dialog_title_icon
                                   : R.string.webapk_update_dialog_title_name;
            explanationId = iconChanging ? R.string.webapk_update_explanation_icon
                                         : R.string.webapk_update_explanation_name;
        }

        WebApkIconNameUpdateCustomView dialogCustomView =
                (WebApkIconNameUpdateCustomView) LayoutInflaterUtils.inflate(
                        context, R.layout.webapk_icon_name_update_dialog, null);
        dialogCustomView.configureIcons(
                currentAppIcon, updatedAppIcon, oldIconAdaptive, newIconAdaptive);
        if (shortNameChanging) {
            dialogCustomView.configureShortNames(oldAppShortName, newAppShortName);
        }
        if (nameChanging) {
            dialogCustomView.configureNames(oldAppName, newAppName);
        }

        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, resources, titleId)
                        .with(ModalDialogProperties.MESSAGE, resources, explanationId)
                        .with(ModalDialogProperties.CUSTOM_VIEW, dialogCustomView)
                        .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, true)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.webapk_update_button_update)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.webapk_update_button_close)
                        .with(ModalDialogProperties.TITLE_SCROLLABLE, true)
                        .build();

        mModalDialogManager = manager;
        mModalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                break;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                break;
            default:
                Log.i(TAG, "Unexpected button pressed in dialog: " + buttonType);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogResultCallback.onResult(dismissalCause);
        mModalDialogManager = null;
    }
}
