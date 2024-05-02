// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.core.util.Function;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog that asks the user if they're certain they want to perform and action. */
public class ActionConfirmationDialog {
    @FunctionalInterface
    public interface ConfirmationDialogResult {
        /**
         * Called when the dialog is dismissed.
         *
         * @param isPositive If a positive action should be taken as a result.
         * @param stopShowing If the user wants to stop showing this dialog in the future.
         */
        void onDismiss(boolean isPositive, boolean stopShowing);
    }

    private final Profile mProfile;
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;

    public ActionConfirmationDialog(
            @NonNull Profile profile,
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager) {
        mProfile = profile;
        mContext = context;
        mModalDialogManager = modalDialogManager;
    }

    public void show(
            @StringRes int titleRes,
            Function<Resources, String> descriptionResolver,
            @StringRes int positiveButtonRes,
            @NonNull ConfirmationDialogResult onResult) {
        Resources resources = mContext.getResources();
        View customView =
                LayoutInflater.from(mContext).inflate(R.layout.action_confirmation_dialog, null);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);

        String descriptionText = descriptionResolver.apply(resources);
        descriptionTextView.setText(descriptionText);

        Callback<Boolean> onDismissWhetherPositive =
                (isPositive) -> {
                    // Only remember to stop showing on the positive case. Otherwise the user may
                    // have just wanted to cancel.
                    boolean shouldStopShowing = isPositive && stopShowingCheckBox.isChecked();
                    onResult.onDismiss(isPositive, shouldStopShowing);
                };
        ModalDialogProperties.Controller dialogController =
                new WasPositiveController(mModalDialogManager, onDismissWhetherPositive);

        String titleText = resources.getString(titleRes);
        String positiveText = resources.getString(positiveButtonRes);
        String negativeText = resources.getString(R.string.cancel);

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, titleText)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveText)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeText)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .build();
        mModalDialogManager.showDialog(model, ModalDialogType.APP);
    }
}
