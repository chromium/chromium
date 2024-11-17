// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.MIGRATE_EXISTING_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.SAVE_NEW_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.UPDATE_EXISTING_ADDRESS_PROFILE;

import android.app.Activity;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Prompt that asks users to confirm saving an address profile imported from a form submission.
 * TODO(crbug.com/40263971): cover with render tests.
 */
@JNINamespace("autofill")
public class SaveUpdateAddressProfilePrompt {
    private final SaveUpdateAddressProfilePromptController mController;
    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mDialogModel;
    private final View mDialogView;
    private AddressEditorCoordinator mAddressEditor;
    private boolean mEditorClosingPending;

    /** Save prompt to confirm saving an address profile imported from a form submission. */
    public SaveUpdateAddressProfilePrompt(
            SaveUpdateAddressProfilePromptController controller,
            ModalDialogManager modalDialogManager,
            Activity activity,
            Profile browserProfile,
            AutofillProfile autofillProfile,
            boolean isUpdate,
            boolean isMigrationToAccount) {
        mController = controller;
        mModalDialogManager = modalDialogManager;

        LayoutInflater inflater = LayoutInflater.from(activity);
        final @UserFlow int userFlow;
        if (isMigrationToAccount) {
            mDialogView = inflater.inflate(R.layout.autofill_migrate_address_profile_prompt, null);
            userFlow = MIGRATE_EXISTING_ADDRESS_PROFILE;
        } else if (isUpdate) {
            mDialogView = inflater.inflate(R.layout.autofill_update_address_profile_prompt, null);
            userFlow = UPDATE_EXISTING_ADDRESS_PROFILE;
        } else {
            mDialogView = inflater.inflate(R.layout.autofill_save_address_profile_prompt, null);
            userFlow = SAVE_NEW_ADDRESS_PROFILE;
        }

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

        Delegate delegate =
                new Delegate() {
                    @Override
                    public void onDone(AutofillAddress address) {
                        onEdited(address);
                    }
                };
        mAddressEditor =
                new AddressEditorCoordinator(
                        activity,
                        delegate,
                        browserProfile,
                        new AutofillAddress(
                                activity,
                                autofillProfile,
                                PersonalDataManagerFactory.getForProfile(browserProfile)),
                        userFlow,
                        /* saveToDisk= */ false);
        mDialogView
                .findViewById(R.id.edit_button)
                .setOnClickListener(
                        v -> {
                            mAddressEditor.showEditorDialog();
                        });
    }

    /** Shows the dialog for saving an address. */
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
    private static @Nullable SaveUpdateAddressProfilePrompt create(
            WindowAndroid windowAndroid,
            SaveUpdateAddressProfilePromptController controller,
            Profile browserProfile,
            AutofillProfile autofillProfile,
            boolean isUpdate,
            boolean isMigrationToAccount) {
        Activity activity = windowAndroid.getActivity().get();
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (activity == null || modalDialogManager == null) return null;

        return new SaveUpdateAddressProfilePrompt(
                controller,
                modalDialogManager,
                activity,
                browserProfile,
                autofillProfile,
                isUpdate,
                isMigrationToAccount);
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
     * Displays an optional notification for the user in case the autofill profile is going to be
     * saved in account storage.
     *
     * @param recordTypeNotice the footer notification for the user.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setRecordTypeNotice(String recordTypeNotice) {
        showTextIfNotEmpty(
                mDialogView.findViewById(R.id.autofill_address_profile_prompt_record_type_notice),
                recordTypeNotice);
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

    /** Dismisses the prompt without returning any user response. */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void dismiss() {
        // Do not dismiss the editor if closing is pending to not abort the animation.
        if (!mEditorClosingPending && mAddressEditor.isShowing()) mAddressEditor.dismiss();
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
        mDialogView
                .findViewById(R.id.no_header_space)
                .setVisibility(show ? View.GONE : View.VISIBLE);
    }

    void setAddressEditorForTesting(AddressEditorCoordinator addressEditor) {
        var oldValue = mAddressEditor;
        mAddressEditor = addressEditor;
        ResettersForTesting.register(() -> mAddressEditor = oldValue);
    }

    View getDialogViewForTesting() {
        return mDialogView;
    }
}
