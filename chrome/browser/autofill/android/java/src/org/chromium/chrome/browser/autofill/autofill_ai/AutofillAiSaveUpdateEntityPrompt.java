// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Prompt that asks users to confirm saving an entity imported from a form submission.
 *
 * <p>TODO: crbug.com/460410690 - Write render tests.
 */
@JNINamespace("autofill")
@NullMarked
public class AutofillAiSaveUpdateEntityPrompt {
    private final AutofillAiSaveUpdateEntityPromptController mController;
    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mDialogModel;
    private final View mDialogView;

    /** Save prompt to confirm saving an entity imported from a form submission. */
    public AutofillAiSaveUpdateEntityPrompt(
            AutofillAiSaveUpdateEntityPromptController controller,
            ModalDialogManager modalDialogManager,
            Context context) {
        mController = controller;
        mModalDialogManager = modalDialogManager;

        LayoutInflater inflater = LayoutInflater.from(context);
        mDialogView = inflater.inflate(R.layout.autofill_ai_save_entity_prompt, null);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        modalDialogManager, this::onDismiss))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView);
        mDialogModel = builder.build();
    }

    /** Shows the dialog for saving an address. */
    @CalledByNative
    @VisibleForTesting
    void show() {
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Creates the prompt for saving an address.
     *
     * @param windowAndroid the window to supply Android dependencies.
     * @param controller the controller to handle the interaction.
     * @return instance of the AutofillAiSaveUpdateEntityPrompt or null if the call failed.
     */
    @CalledByNative
    private static @Nullable AutofillAiSaveUpdateEntityPrompt create(
            WindowAndroid windowAndroid, AutofillAiSaveUpdateEntityPromptController controller) {
        @Nullable Activity activity = windowAndroid.getActivity().get();
        @Nullable ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (activity == null || modalDialogManager == null) return null;

        return new AutofillAiSaveUpdateEntityPrompt(controller, modalDialogManager, activity);
    }

    /**
     * Displays the dialog-specific properties.
     *
     * @param title the title of the dialog.
     * @param positiveButtonText the text on the positive button.
     * @param negativeButtonText the text on the negative button.
     */
    @CalledByNative
    @VisibleForTesting
    void setDialogDetails(String title, String positiveButtonText, String negativeButtonText) {
        mDialogModel.set(ModalDialogProperties.TITLE, title);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText);
        mDialogModel.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonText);
    }

    /** Dismisses the prompt without returning any user response. */
    @CalledByNative
    @VisibleForTesting
    void dismiss() {
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
            case DialogDismissalCause.ACTION_ON_CONTENT:
            default:
                // No explicit user decision.
                break;
        }
        mController.onPromptDismissed();
    }

    View getDialogViewForTesting() {
        return mDialogView;
    }
}
