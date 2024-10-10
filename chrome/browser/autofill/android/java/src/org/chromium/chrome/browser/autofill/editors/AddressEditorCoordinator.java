// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VISIBLE;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An address editor. Can be used for either shipping or billing address editing. */
public class AddressEditorCoordinator {
    private final AddressEditorMediator mMediator;
    private EditorDialogView mEditorDialog;
    @Nullable private PropertyModel mEditorModel;

    /** Delegate used to subscribe to AddressEditor user interactions. */
    public static interface Delegate {
        /**
         * The user has tapped "Done" button.
         *
         * @param autofillAddress the autofill address with all user changes.
         */
        default void onDone(AutofillAddress autofillAddress) {}

        /** The user has canceled editing the address. */
        default void onCancel() {}

        /**
         * The user has confirmed deletion of this autofill profile.
         *
         * @param autofillAddress the initial autofill address with no user changes.
         */
        default void onDelete(AutofillAddress autofillAddress) {}
    }

    /** Different types of user flows this editor supports. */
    @IntDef({
        UserFlow.CREATE_NEW_ADDRESS_PROFILE,
        UserFlow.SAVE_NEW_ADDRESS_PROFILE,
        UserFlow.UPDATE_EXISTING_ADDRESS_PROFILE,
        UserFlow.MIGRATE_EXISTING_ADDRESS_PROFILE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface UserFlow {
        // The user creates a new address from Chrome settings.
        int CREATE_NEW_ADDRESS_PROFILE = 1;
        // The user edits an potentially save an address parsed from a submitted form.
        int SAVE_NEW_ADDRESS_PROFILE = 2;
        // The user edits an existing address either from Chrome settings or upon form submission.
        int UPDATE_EXISTING_ADDRESS_PROFILE = 3;
        // The user edits an existing
        int MIGRATE_EXISTING_ADDRESS_PROFILE = 4;
    }

    /**
     * Builds an address editor for a new address profile.
     *
     * @param activity The activity on top of which the UI should be displayed.
     * @param delegate Delegate to react to users interactions with the editor.
     * @param profile Current user's profile.
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditorCoordinator(
            Activity activity, Delegate delegate, Profile profile, boolean saveToDisk) {
        this(
                activity,
                delegate,
                profile,
                new AutofillAddress(
                        activity,
                        AutofillProfile.builder().build(),
                        PersonalDataManagerFactory.getForProfile(profile)),
                UserFlow.CREATE_NEW_ADDRESS_PROFILE,
                saveToDisk);
    }

    /**
     * Builds an address editor for an existing address profile.
     *
     * @param activity The activity on top of which the UI should be displayed.
     * @param delegate Delegate to react to users interactions with the editor.
     * @param profile Current user's profile.
     * @param addressToEdit Address the user wants to modify.
     * @param userFlow the current user flow this editor is used for.
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditorCoordinator(
            Activity activity,
            Delegate delegate,
            Profile profile,
            AutofillAddress addressToEdit,
            @UserFlow int userFlow,
            boolean saveToDisk) {
        mMediator =
                new AddressEditorMediator(
                        activity,
                        delegate,
                        IdentityServicesProvider.get().getIdentityManager(profile),
                        SyncServiceFactory.getForProfile(profile),
                        PersonalDataManagerFactory.getForProfile(profile),
                        addressToEdit,
                        userFlow,
                        saveToDisk);
        mEditorDialog = new EditorDialogView(activity, profile);
    }

    /**
     * Sets the custom text to be shown on the done button.
     *
     * @param customDoneButtonText The text to display on the done button. If null, the default
     *     value will be used.
     */
    public void setCustomDoneButtonText(@Nullable String customDoneButtonText) {
        mMediator.setCustomDoneButtonText(customDoneButtonText);
    }

    /**
     * Sets the runnable deleting the current autofill profile, e.g. when the user selects the
     * delete option in the menu and confirms autofill profile deletion.
     */
    public void setAllowDelete(boolean allowDelete) {
        mMediator.setAllowDelete(allowDelete);
    }

    /** Notifies underlying view that device configuration has changed. */
    public void onConfigurationChanged() {
        mEditorDialog.onConfigurationChanged();
    }

    /** Shows editor dialog to the user. */
    public void showEditorDialog() {
        mEditorModel = mMediator.getEditorModel();
        PropertyModelChangeProcessor.create(
                mEditorModel, mEditorDialog, EditorDialogViewBinder::bindEditorDialogView);
        mEditorModel.set(VISIBLE, true);
    }

    /**
     * Check if current editor dialog is visible to the user.
     *
     * @return true if this editor is visible to the user, false otherwise.
     */
    public boolean isShowing() {
        return mEditorDialog.isShowing();
    }

    /** Dismiss currently visible editor dialog. */
    public void dismiss() {
        mEditorDialog.dismiss();
    }

    /**
     * @param editorDialog Test version of the editor dialog.
     */
    void setEditorDialogForTesting(EditorDialogView editorDialog) {
        mEditorDialog = editorDialog;
    }

    /**
     * @return editor dialog model for testing purposes.
     */
    @Nullable
    PropertyModel getEditorModelForTesting() {
        return mEditorModel;
    }

    /**
     * @return editor dialog view for testing purposes.
     */
    public EditorDialogView getEditorDialogForTesting() {
        return mEditorDialog;
    }
}
