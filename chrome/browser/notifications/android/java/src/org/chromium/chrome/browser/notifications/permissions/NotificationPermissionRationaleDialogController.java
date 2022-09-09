// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.NotificationRationaleResult;
import org.chromium.chrome.browser.notifications.R;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleDelegate;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog to explain the advantages of Chrome notifications.
 */
public class NotificationPermissionRationaleDialogController implements RationaleDelegate {
    public static final String DIALOG_TEXT_VARIANT_2 =
            "notification_permission_dialog_text_variant_2";
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;

    /**
     * Initializes the notification permission rationale dialog.
     * @param context Used to inflate views and get resources.
     * @param modalDialogManager Modal manager used to show the dialog and listen to its results.
     */
    public NotificationPermissionRationaleDialogController(
            Context context, ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;
        mContext = context;
    }

    /**
     * Shows the notification permission rationale dialog.
     * @param rationaleCallback A callback indicating whether the user accepted to enable
     * notifications.
     */
    @Override
    public void showRationaleUi(Callback<Boolean> rationaleCallback) {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        Resources resources = mContext.getResources();

        View dialogView = inflater.inflate(R.layout.notification_permission_rationale_dialog,
                /* root= */ null);
        TextView titleView = dialogView.findViewById(R.id.notification_permission_rationale_title);
        TextView descriptionView =
                dialogView.findViewById(R.id.notification_permission_rationale_message);
        boolean shouldShowVariant2 = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT, DIALOG_TEXT_VARIANT_2, false);
        titleView.setText(shouldShowVariant2
                        ? R.string.notification_permission_rationale_dialog_title_variation_2
                        : R.string.notification_permission_rationale_dialog_title);
        descriptionView.setText(shouldShowVariant2
                        ? R.string.notification_permission_rationale_dialog_message_variation_2
                        : R.string.notification_permission_rationale_dialog_message);

        PropertyModel.Builder dialogModelBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(mModalDialogManager,
                                        wrapDialogDismissalCallback(rationaleCallback)))
                        .with(ModalDialogProperties.CUSTOM_VIEW, dialogView)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.notification_permission_rationale_accept_button_text)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.notification_permission_rationale_reject_button_text);

        PropertyModel dialogModel = dialogModelBuilder.build();

        mModalDialogManager.showDialog(dialogModel, ModalDialogType.APP);
    }

    private Callback<Integer> wrapDialogDismissalCallback(Callback<Boolean> rationaleCallback) {
        return result -> {
            @NotificationRationaleResult
            int resultEnumValue;

            switch (result) {
                case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                    resultEnumValue = NotificationRationaleResult.POSITIVE_BUTTON_CLICKED;
                    break;
                case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                    resultEnumValue = NotificationRationaleResult.NEGATIVE_BUTTON_CLICKED;
                    break;
                case DialogDismissalCause.ACTIVITY_DESTROYED:
                    resultEnumValue = NotificationRationaleResult.ACTIVITY_DESTROYED;
                    break;
                case DialogDismissalCause.NOT_ATTACHED_TO_WINDOW:
                    resultEnumValue = NotificationRationaleResult.NOT_ATTACHED_TO_WINDOW;
                    break;
                case DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE:
                default:
                    resultEnumValue = NotificationRationaleResult.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;
            }

            NotificationUmaTracker.getInstance().onNotificationPermissionRationaleResult(
                    resultEnumValue);
            rationaleCallback.onResult(result == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        };
    }
}
