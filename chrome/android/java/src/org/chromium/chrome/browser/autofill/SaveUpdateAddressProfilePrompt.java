// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.settings.AddressEditor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Prompt that asks users to confirm saving an address profile imported from a form submission.
 * TODO(crbug.com/1432549): cover with render tests.
 */
@JNINamespace("autofill")
@JNIAdditionalImport(PersonalDataManager.class)
public class SaveUpdateAddressProfilePrompt {
    private final SaveUpdateAddressProfilePromptController mController;
    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mDialogModel;
    private final View mDialogView;
    private final EditorDialog mEditorDialog;
    private AddressEditor mAddressEditor;
    private boolean mEditorClosingPending;

    /**
     * Save prompt to confirm saving an address profile imported from a form submission.
     */
    public SaveUpdateAddressProfilePrompt(SaveUpdateAddressProfilePromptController controller,
            ModalDialogManager modalDialogManager, Activity activity, Profile browserProfile,
            PersonalDataManager.AutofillProfile autofillProfile, boolean isUpdate,
            boolean isMigrationToAccount) {
        mController = controller;
        mModalDialogManager = modalDialogManager;

        LayoutInflater inflater = LayoutInflater.from(activity);
        if (isMigrationToAccount) {
            mDialogView = inflater.inflate(R.layout.autofill_migrate_address_profile_prompt, null);
        } else if (isUpdate) {
            mDialogView = inflater.inflate(R.layout.autofill_update_address_profile_prompt, null);
        } else {
            mDialogView = inflater.inflate(R.layout.autofill_save_address_profile_prompt, null);
        }

        if (!isUpdate && !isMigrationToAccount) setupAddressNickname();

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        modalDialogManager, this::onDismiss))
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView);
        mDialogModel = builder.build();

        mEditorDialog = new EditorDialog(
                activity, /*deleteRunnable=*/null, browserProfile, /*requiredIndicator=*/false);
        mEditorDialog.setShouldTriggerDoneCallbackBeforeCloseAnimation(true);
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false, isUpdate, isMigrationToAccount);
        mAddressEditor.setEditorDialog(mEditorDialog);
        AutofillAddress autofillAddress = new AutofillAddress(activity, autofillProfile);
        mDialogView.findViewById(R.id.edit_button).setOnClickListener(v -> {
            mAddressEditor.edit(autofillAddress, /*doneCallback=*/this::onEdited,
                    /*cancelCallback=*/unused -> {});
        });
    }

    /**
     * Shows the dialog for saving an address.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void show() {
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Creates the prompt for saving an address.
     *
     * @param windowAndroid the window to supply Android dependencies.
     * @param controller the controller to handle the interaction.
     * @param browserProfile the Chrome profile being used.
     * @param autofillProfile the address data to be saved.
     * @param isUpdate true if there's an existing profile which will be updated, false otherwise.
     * @param isMigrationToAccount true if address profile is going to be saved in user's Google
     *         account, false otherwise.
     * @return instance of the SaveUpdateAddressProfilePrompt or null if the call failed.
     */
    @CalledByNative
    @Nullable
    private static SaveUpdateAddressProfilePrompt create(WindowAndroid windowAndroid,
            SaveUpdateAddressProfilePromptController controller, Profile browserProfile,
            PersonalDataManager.AutofillProfile autofillProfile, boolean isUpdate,
            boolean isMigrationToAccount) {
        Activity activity = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (activity == null || modalDialogManager == null) return null;

        return new SaveUpdateAddressProfilePrompt(controller, modalDialogManager, activity,
                browserProfile, autofillProfile, isUpdate, isMigrationToAccount);
    }

    /**
     * Displays the dialog-specific properties.
     *
     * @param title the title of the dialog.
     * @param positiveButtonText the text on the positive button.
     * @param negativeButtonText the text on the negative button.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setDialogDetails(String title, String positiveButtonText, String negativeButtonText) {
        mDialogModel.set(ModalDialogProperties.TITLE, title);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText);
        mDialogModel.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonText);
        // The text in the editor should match the text in the dialog.
        mAddressEditor.setCustomDoneButtonText(positiveButtonText);
    }

    /**
     * Displays an optional notification for the user in case the autofill profile is going
     * to be saved in account storage.
     *
     * @param sourceNotice the footer notification for the user.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setSourceNotice(String sourceNotice) {
        showTextIfNotEmpty(
                mDialogView.findViewById(R.id.autofill_address_profile_prompt_source_notice),
                sourceNotice);
    }

    /**
     * Displays the details in case a new address to be saved.
     *
     * @param address the address details to be saved.
     * @param email the email to be saved.
     * @param phone the phone to be saved.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setSaveOrMigrateDetails(String address, String email, String phone) {
        showTextIfNotEmpty(mDialogView.findViewById(R.id.address), address);
        showTextIfNotEmpty(mDialogView.findViewById(R.id.email), email);
        showTextIfNotEmpty(mDialogView.findViewById(R.id.phone), phone);
    }

    /**
     * Displays the details in case an existing address to be updated. If oldDetails are empty, only
     * newDetails are shown.
     *
     * @param subtitle the text to display below the title.
     * @param oldDetails details in the existing profile that differ.
     * @param newDetails details in the new profile that differ.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setUpdateDetails(String subtitle, String oldDetails, String newDetails) {
        showTextIfNotEmpty(mDialogView.findViewById(R.id.subtitle), subtitle);
        showHeaders(!TextUtils.isEmpty(oldDetails));
        showTextIfNotEmpty(mDialogView.findViewById(R.id.details_old), oldDetails);
        showTextIfNotEmpty(mDialogView.findViewById(R.id.details_new), newDetails);
    }

    /**
     * Dismisses the prompt without returning any user response.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void dismiss() {
        // Do not dismiss the editor if closing is pending to not abort the animation.
        if (!mEditorClosingPending && mEditorDialog.isShowing()) mEditorDialog.dismiss();
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onEdited(AutofillAddress autofillAddress) {
        mEditorClosingPending = true;
        mController.onUserEdited(autofillAddress.getProfile());
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
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

    private void showTextIfNotEmpty(TextView textView, String text) {
        if (TextUtils.isEmpty(text)) {
            textView.setVisibility(View.GONE);
        } else {
            textView.setVisibility(View.VISIBLE);
            textView.setText(text);
        }
    }

    private void showHeaders(boolean show) {
        mDialogView.findViewById(R.id.header_new).setVisibility(show ? View.VISIBLE : View.GONE);
        mDialogView.findViewById(R.id.header_old).setVisibility(show ? View.VISIBLE : View.GONE);
        mDialogView.findViewById(R.id.no_header_space)
                .setVisibility(show ? View.GONE : View.VISIBLE);
    }

    private void setupAddressNickname() {
        TextInputLayout nicknameInputLayout = mDialogView.findViewById(R.id.nickname_input_layout);
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT)) {
            nicknameInputLayout.setVisibility(View.GONE);
            return;
        }
        EditText nicknameInput = mDialogView.findViewById(R.id.nickname_input);
        nicknameInput.setOnFocusChangeListener(
                (v, hasFocus)
                        -> nicknameInputLayout.setHint(
                                !hasFocus && TextUtils.isEmpty(nicknameInput.getText())
                                        // TODO(crbug.com/1167061): Use localized strings.
                                        ? "Add a label"
                                        : "Label"));

        // Prevent input from being focused when keyboard is closed.
        KeyboardVisibilityDelegate.getInstance().addKeyboardVisibilityListener(isShowing -> {
            if (!isShowing && nicknameInput.hasFocus()) nicknameInput.clearFocus();
        });
    }

    void setAddressEditorForTesting(AddressEditor addressEditor) {
        mAddressEditor = addressEditor;
    }

    View getDialogViewForTesting() {
        return mDialogView;
    }
}
