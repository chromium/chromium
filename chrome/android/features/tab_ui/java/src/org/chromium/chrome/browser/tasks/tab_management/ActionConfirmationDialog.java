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
import org.chromium.chrome.browser.tasks.tab_management.StrictButtonPressController.ButtonClickResult;
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
         * @param buttonClickResult The button click result from the dialog.
         * @param stopShowing If the user wants to stop showing this dialog in the future.
         */
        void onDismiss(@ButtonClickResult int buttonClickResult, boolean stopShowing);
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;

    /**
     * @param context The context to use for resources.
     * @param modalDialogManager The global modal dialog manager.
     */
    public ActionConfirmationDialog(
            @NonNull Context context, @NonNull ModalDialogManager modalDialogManager) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * Shows an action confirmation dialog.
     *
     * @param titleResolver Resolves a title for the dialog.
     * @param descriptionResolver Resolves a description for the dialog.
     * @param positiveButtonRes The string to show for the positive button.
     * @param negativeButtonRes The string to show for the negative button.
     * @param supportStopShowing Whether to show a checkbox to permanently disable the dialog via a
     *     pref.
     * @param onResult The callback to invoke on exit of the dialog.
     */
    public void show(
            Function<Resources, String> titleResolver,
            Function<Resources, String> descriptionResolver,
            @StringRes int positiveButtonRes,
            @StringRes int negativeButtonRes,
            boolean supportStopShowing,
            @NonNull ConfirmationDialogResult onResult) {
        Resources resources = mContext.getResources();

        View customView =
                LayoutInflater.from(mContext).inflate(R.layout.action_confirmation_dialog, null);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setVisibility(supportStopShowing ? View.VISIBLE : View.GONE);

        String descriptionText = descriptionResolver.apply(resources);
        descriptionTextView.setText(descriptionText);

        Callback<Integer> onButtonClick =
                (buttonClickResult) -> {
                    // Only remember to stop showing when a button is clicked. Otherwise the user
                    // may have just wanted to cancel.
                    boolean shouldStopShowing =
                            buttonClickResult != ButtonClickResult.NO_CLICK
                                    && stopShowingCheckBox.isChecked();
                    onResult.onDismiss(buttonClickResult, shouldStopShowing);
                };
        ModalDialogProperties.Controller dialogController =
                new StrictButtonPressController(mModalDialogManager, onButtonClick);

        String titleText = titleResolver.apply(resources);
        String positiveText = resources.getString(positiveButtonRes);
        String negativeText = resources.getString(negativeButtonRes);

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
