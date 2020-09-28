// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.RadioGroup;

import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog for the "Get Image Descriptions" feature. If a user is a screen reader user, they will
 * see a new option under the main menu to get image descriptions. If they select that option this
 * dialog will display giving the user the option to enable the feature.
 */
public class ImageDescriptionsDialog
        implements ModalDialogProperties.Controller, RadioGroup.OnCheckedChangeListener {
    /**
     * A delegate to respond to actions taken in the dialog.
     */
    public interface Delegate {
        void enableImageDescriptions(boolean onlyOnWifi);
        void getImageDescriptionsJustOnce(boolean dontAskAgain);
    }

    private ImageDescriptionsDialog.Delegate mControllerDelegate;

    private ModalDialogManager mModalDialogManager;
    private PropertyModel mPropertyModel;

    private RadioButtonWithDescriptionLayout mRadioGroup;
    private RadioButtonWithDescription mOptionJustOnceRadioButton;
    private RadioButtonWithDescription mOptionAlwaysRadioButton;
    private CheckBox mOptionalCheckbox;

    private boolean mShouldShowDontAskAgainOption;
    private boolean mOnlyOnWifiState;
    private boolean mDontAskAgainState;

    protected ImageDescriptionsDialog(Context context, ModalDialogManager modalDialogManager,
            ImageDescriptionsDialog.Delegate delegate, boolean shouldShowDontAskAgainOption) {
        mModalDialogManager = modalDialogManager;
        mControllerDelegate = delegate;

        // Set initial state.
        mShouldShowDontAskAgainOption = shouldShowDontAskAgainOption;
        mOnlyOnWifiState = true;
        mDontAskAgainState = false;

        // Inflate our custom view layout for this dialog.
        LayoutInflater inflater = LayoutInflater.from(context);
        View rootView = inflater.inflate(R.layout.image_descriptions_dialog, null);

        mRadioGroup = rootView.findViewById(R.id.image_descriptions_dialog_radio_button_group);
        mRadioGroup.setOnCheckedChangeListener(this);

        mOptionJustOnceRadioButton =
                rootView.findViewById(R.id.image_descriptions_dialog_radio_button_just_once);
        mOptionAlwaysRadioButton =
                rootView.findViewById(R.id.image_descriptions_dialog_radio_button_always);

        mOptionalCheckbox = rootView.findViewById(R.id.image_descriptions_dialog_check_box);
        mOptionalCheckbox.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (mOptionJustOnceRadioButton.isChecked()) {
                mDontAskAgainState = isChecked;
            } else {
                mOnlyOnWifiState = isChecked;
            }
        });

        // Dialog should start with "Just once" checked, and with optional checkbox as needed.
        mOptionJustOnceRadioButton.setChecked(true);
        if (mShouldShowDontAskAgainOption) {
            updateOptionalCheckbox(R.string.dont_ask_again, mDontAskAgainState);
        }

        // Build our dialog property model.
        mPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, context.getResources(),
                                R.string.image_descriptions_dialog_header)
                        .with(ModalDialogProperties.CUSTOM_VIEW, rootView)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                                R.string.no_thanks)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, context.getResources(),
                                R.string.image_descriptions_dialog_get_descriptions_button)
                        .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, true)
                        .build();
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        // When the "Always" option is checked, we display the choice for "Only on Wi-Fi", we
        // hide this checkbox when user has selected "Just once" and optionally display a
        // "Don't ask again" checkbox if the user has chosen just once enough times.
        if (checkedId == mOptionAlwaysRadioButton.getId()) {
            updateOptionalCheckbox(
                    R.string.image_descriptions_dialog_option_only_on_wifi, mOnlyOnWifiState);
        } else if (checkedId == mOptionJustOnceRadioButton.getId()) {
            if (mShouldShowDontAskAgainOption) {
                updateOptionalCheckbox(R.string.dont_ask_again, mDontAskAgainState);
            } else {
                mOptionalCheckbox.setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        int dismissalCause;

        // User has elected to get image descriptions
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            // Determine desired level of descriptions and default to just once
            if (mOptionAlwaysRadioButton.isChecked()) {
                mControllerDelegate.enableImageDescriptions(mOnlyOnWifiState);
            } else if (mOptionJustOnceRadioButton.isChecked()) {
                mControllerDelegate.getImageDescriptionsJustOnce(mDontAskAgainState);
            }

            dismissalCause = DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
        } else {
            dismissalCause = DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
        }

        // Dismiss the dialog.
        dismiss(dismissalCause);
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {}

    /**
     * Helper method to set the optional checkbox text, visibility, and checked state
     * @param textId    ID of string for primary text
     * @param state     Checked state of optional checkbox
     */
    private void updateOptionalCheckbox(int textId, boolean state) {
        mOptionalCheckbox.setVisibility(View.VISIBLE);
        mOptionalCheckbox.setText(textId);
        mOptionalCheckbox.setChecked(state);
    }

    /**
     * Helper method to display this dialog.
     */
    public void show() {
        mModalDialogManager.showDialog(mPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Helper method to dismiss this dialog.
     * @param dialogDismissableCause        DialogDismissalCause, e.g. positive or negative
     */
    public void dismiss(int dialogDismissableCause) {
        mModalDialogManager.dismissDialog(mPropertyModel, dialogDismissableCause);
    }
}
