// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.provider.Settings;
import android.view.View;
import android.widget.CheckBox;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The uninstall confirmation dialog, which allows the user to confirm that they want to uninstall
 * and report the app as abusive.
 */
@NullMarked
public class WebApkUpdateReportAbuseDialog implements ModalDialogProperties.Controller {
    /** Interface for receiving notifications of user actions. */
    public interface Callback {
        /** Called when the user has selected to uninstall the app. */
        void onUninstall();
    }

    private static final String TAG = "UpdateReportAbuseDlg";

    // The Activity context to use.
    private final Context mActivityContext;

    // The modal dialog manager to use.
    private final ModalDialogManager mModalDialogManager;

    // The short name of the app the user is uninstalling.
    private final @Nullable String mAppShortName;

    // The package name for the app the user is uninstalling.
    private final @Nullable String mAppPackageName;

    // Whether to show the checkbox for reporting abuse.
    private final boolean mShowAbuseCheckbox;

    // When checked, the app will not just be uninstalled, but also reported for abuse.
    private @Nullable CheckBox mReportAbuseCheckBox;

    // Notifies the parent (dialog beneath us) that uninstalling was the action taken by the user.
    private final Callback mOnUninstallCallback;

    public WebApkUpdateReportAbuseDialog(
            Context activityContext,
            ModalDialogManager manager,
            @Nullable String appPackageName,
            @Nullable String appShortName,
            boolean showAbuseCheckbox,
            Callback callback) {
        mActivityContext = activityContext;
        mModalDialogManager = manager;
        mAppPackageName = appPackageName;
        mAppShortName = appShortName;
        mShowAbuseCheckbox = showAbuseCheckbox;
        mOnUninstallCallback = callback;
    }

    /** Shows the dialog. */
    public void show() {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        String title =
                resources.getString(R.string.webapk_report_abuse_dialog_title, mAppShortName);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.webapk_report_abuse_confirm)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.webapk_report_abuse_cancel);
        if (mShowAbuseCheckbox) {
            View dialogCustomView =
                    LayoutInflaterUtils.inflate(
                            context, R.layout.webapk_update_report_abuse_custom_view, null);
            mReportAbuseCheckBox = dialogCustomView.findViewById(R.id.report_abuse);
            builder = builder.with(ModalDialogProperties.CUSTOM_VIEW, dialogCustomView);
        }
        PropertyModel dialogModel = builder.build();

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
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            mOnUninstallCallback.onUninstall();

            if (mReportAbuseCheckBox != null && mReportAbuseCheckBox.isChecked()) {
                // TODO(finnur): Implement sending info to the SafeBrowsing team.
                Log.i(TAG, "Send report to SafeBrowsing");
            }

            showAppInfoToUninstall();
        }
    }

    private void showAppInfoToUninstall() {
        assert mAppPackageName != null;

        if (!PackageUtils.isPackageInstalled(mAppPackageName)) {
            Log.i(TAG, "WebApk not found: " + mAppPackageName);
            return;
        }

        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setData(Uri.parse("package:" + mAppPackageName));
        mActivityContext.startActivity(intent);
    }
}
