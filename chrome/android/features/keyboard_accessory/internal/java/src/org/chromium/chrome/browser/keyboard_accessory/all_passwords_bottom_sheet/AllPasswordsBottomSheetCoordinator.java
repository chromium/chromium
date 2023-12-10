// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates the AllPasswordsBottomSheet. AllPasswordsBottomSheet uses a bottom sheet to let the
 * user select a credential and fills it into the focused form.
 */
class AllPasswordsBottomSheetCoordinator {
    private final AllPasswordsBottomSheetMediator mMediator = new AllPasswordsBottomSheetMediator();

    /**
     * This delegate is called when the AllPasswordsBottomSheet is interacted with (e.g. dismissed
     * or a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user selects one of the credentials shown in the AllPasswordsBottomSheet.
         */
        void onCredentialSelected(CredentialFillRequest credentialFillRequest);

        /**
         * Called when the user dismisses the AllPasswordsBottomSheet or if the bottom sheet content
         * failed to be shown.
         */
        void onDismissed();
    }

    /**
     * Initializes the component.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles select and dismiss events.
     * @param origin The origin for the current focused frame.
     */
    public void initialize(
            Context context,
            BottomSheetController sheetController,
            AllPasswordsBottomSheetCoordinator.Delegate delegate,
            String origin) {
        PropertyModel model =
                AllPasswordsBottomSheetProperties.createDefaultModel(
                        origin, mMediator::onDismissed, mMediator::onQueryTextChange);
        mMediator.initialize(delegate, model);
        setUpModelChangeProcessor(model, new AllPasswordsBottomSheetView(context, sheetController));
    }

    /**
     * Displays the given credentials in a new bottom sheet.
     * @param credentials An array of {@link Credential}s that will be displayed.
     * @param isPasswordField True if the currently focused field is a password field and false for
     *         any other field type (e.g username, ...).
     */
    public void showCredentials(Credential[] credentials, boolean isPasswordField) {
        mMediator.showCredentials(credentials, isPasswordField);
    }

    @VisibleForTesting
    static void setUpModelChangeProcessor(PropertyModel model, AllPasswordsBottomSheetView view) {
        PropertyModelChangeProcessor.create(
                model, view, AllPasswordsBottomSheetViewBinder::bindAllPasswordsBottomSheet);
    }
}
