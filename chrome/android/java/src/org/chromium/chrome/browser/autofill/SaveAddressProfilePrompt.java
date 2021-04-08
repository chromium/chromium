// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Prompt that asks users to confirm saving an address profile imported from a form submission.
 */
@JNINamespace("autofill")
public class SaveAddressProfilePrompt {
    private final SaveAddressProfilePromptController mController;
    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mDialogModel;

    /**
     * Save prompt to confirm saving an address profile imported from a form submission.
     */
    public SaveAddressProfilePrompt(SaveAddressProfilePromptController controller,
            ModalDialogManager modalDialogManager, Resources resources) {
        mController = controller;
        mModalDialogManager = modalDialogManager;

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        modalDialogManager, this::onDismiss))
                        // TODO(crbug.com/1167061): Use proper localized string.
                        .with(ModalDialogProperties.TITLE, "Save address?")
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.save)
                        .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, true)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.no_thanks)
                        // TODO(crbug.com/1167061): Revisit whether the dialog should be modal.
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false);
        mDialogModel = builder.build();
    }

    /**
     * Shows the dialog for saving an address.
     */
    public void show() {
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Creates and shows the prompt for saving an address.
     *
     * @param windowAndroid the window to supply Android dependencies.
     * @param controller the controller to handle the interaction.
     * @return instance of the SaveAddressProfilePrompt which is now being shown or null if the call
     * failed.
     */
    @CalledByNative
    @Nullable
    private static SaveAddressProfilePrompt show(
            WindowAndroid windowAndroid, SaveAddressProfilePromptController controller) {
        Activity activity = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (activity == null || modalDialogManager == null) return null;

        SaveAddressProfilePrompt prompt = new SaveAddressProfilePrompt(
                controller, modalDialogManager, activity.getResources());
        prompt.show();
        return prompt;
    }

    /**
     * Dismisses the prompt without returning any user response.
     */
    @CalledByNative
    private void dismiss() {
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onDismiss(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                mController.onUserAccepted();
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                mController.onUserDeclined();
                break;
            default:
                // No explicit user decision.
                break;
        }
        mController.onPromptDismissed();
    }
}
