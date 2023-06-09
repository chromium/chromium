// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.profiles.Profile;

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
     * Builds an address editor for a new address profile.
     *
     * @param editorDialog Editor's view displayed to the user.
     * @param delegate Delegate to react to users interactions with the editor.
     * @param profile Current user's profile.
     * @param saveToDisk Whether to save changes to disk after editing.
     * @param isUpdate Whether an existing address profile is being edited.
     * @param isMigrationToAccount Whether this editor is shown during address profile migration to
     *         Google account.
     */
    public AddressEditor(EditorDialog editorDialog, Delegate delegate, Profile profile,
            boolean saveToDisk, boolean isUpdate, boolean isMigrationToAccount) {
        this(editorDialog, delegate, profile,
                new AutofillAddress(editorDialog.getContext(), AutofillProfile.builder().build()),
                saveToDisk, isUpdate, isMigrationToAccount, true);
    }

    /**
     * Builds an address editor for an existing address profile.
     *
     * @param editorDialog Editor's view displayed to the user.
     * @param delegate Delegate to react to users interactions with the editor.
     * @param profile Current user's profile.
     * @param addressToEdit Address the user wants to modify.
     * @param saveToDisk Whether to save changes to disk after editing.
     * @param isUpdate Whether an existing address profile is being edited.
     * @param isMigrationToAccount Whether this editor is shown during address profile migration to
     *         Google account.
     */
    public AddressEditor(EditorDialog editorDialog, Delegate delegate, Profile profile,
            AutofillAddress addressToEdit, boolean saveToDisk, boolean isUpdate,
            boolean isMigrationToAccount) {
        this(editorDialog, delegate, profile, addressToEdit, saveToDisk, isUpdate,
                isMigrationToAccount, false);
    }

    /**
     * Builds an address editor for an existing address profile.
     *
     * @param editorDialog Editor's view displayed to the user.
     * @param delegate Delegate to react to users interactions with the editor.
     * @param profile Current user's profile.
     * @param addressToEdit Address the user wants to modify.
     * @param saveToDisk Whether to save changes to disk after editing.
     * @param isUpdate Whether an existing address profile is being edited.
     * @param isMigrationToAccount Whether this editor is shown during address profile migration to
     *         Google account.
     * @param isProfileNew whether the user intends to create a new address.
     */
    private AddressEditor(EditorDialog editorDialog, Delegate delegate, Profile profile,
            AutofillAddress addressToEdit, boolean saveToDisk, boolean isUpdate,
            boolean isMigrationToAccount, boolean isProfileNew) {
        mMediator = new AddressEditorMediator();
        mMediator.initialize(editorDialog.getContext(), delegate, profile, addressToEdit,
                saveToDisk, isUpdate, isMigrationToAccount, isProfileNew);
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
