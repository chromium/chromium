// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.profiles.Profile;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * An address editor. Can be used for either shipping or billing address editing.
 */
public class AddressEditor {
    private final AddressEditorMediator mMediator;
    private final EditorDialog mEditorDialog;

    /**
     * Delegate used to subscribe to AddressEditor user interactions.
     */
    public static interface Delegate {
        // The user has tapped "Done" button.
        default void onDone(AutofillAddress autofillAddress) {}

        // The user has canceled editing the address.
        default void onCancel() {}
    }

    /**
     * Different types of user flows this editor supports.
     */
    @IntDef({UserFlow.CREATE_NEW_ADDRESS_PROFILE, UserFlow.SAVE_NEW_ADDRESS_PROFILE,
            UserFlow.UPDATE_EXISTING_ADDRESS_PROFILE, UserFlow.MIGRATE_EXISTING_ADDRESS_PROFILE})
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
     * @param editorDialog Editor's view displayed to the user.
     * @param delegate Delegate to react to users interactions with the editor.
     * @param profile Current user's profile.
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditor(
            EditorDialog editorDialog, Delegate delegate, Profile profile, boolean saveToDisk) {
        this(editorDialog, delegate, profile,
                new AutofillAddress(editorDialog.getContext(), AutofillProfile.builder().build()),
                UserFlow.CREATE_NEW_ADDRESS_PROFILE, saveToDisk);
    }

    /**
     * Builds an address editor for an existing address profile.
     *
     * @param editorDialog Editor's view displayed to the user.
     * @param delegate Delegate to react to users interactions with the editor.
     * @param profile Current user's profile.
     * @param addressToEdit Address the user wants to modify.
     * @param userFlow
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditor(EditorDialog editorDialog, Delegate delegate, Profile profile,
            AutofillAddress addressToEdit, @UserFlow int userFlow, boolean saveToDisk) {
        mMediator = new AddressEditorMediator(
                editorDialog.getContext(), delegate, profile, addressToEdit, userFlow, saveToDisk);
        mEditorDialog = editorDialog;
    }

    /**
     * Sets the custom text to be shown on the done button.
     *
     * @param customDoneButtonText The text to display on the done button. If null, the default
     *        value will be used.
     */
    public void setCustomDoneButtonText(@Nullable String customDoneButtonText) {
        mMediator.setCustomDoneButtonText(customDoneButtonText);
    }

    /**
     * Shows editor dialog to the user.
     */
    public void showEditorDialog() {
        mEditorDialog.show(mMediator.buildEditorModel());
    }
}
