// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Maps {@link PasswordEditDialogProperties} changes in a {@link PropertyModel}
 * to suitable methods in {@link PasswordEditDialogView}
 */
class PasswordEditDialogViewBinder {
    static void bind(
            PropertyModel model, PasswordEditDialogView dialogView, PropertyKey propertyKey) {
        if (propertyKey == PasswordEditDialogProperties.USERNAMES) {
            setUsernames(model, dialogView);
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME) {
            assert dialogView instanceof PasswordEditDialogWithDetailsView;
            assert ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS);
            ((PasswordEditDialogWithDetailsView) dialogView)
                    .setUsername(model.get(PasswordEditDialogProperties.USERNAME));
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME_INDEX) {
            assert dialogView instanceof UsernameSelectionConfirmationView;
            assert !ChromeFeatureList.isEnabled(
                    ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS);
            // Propagation of USERNAMES property triggers passing both USERNAMES and
            // USERNAME_INDEX properties to the view. This is safe because both properties
            // are set through property model builder and available by the time the property model
            // is bound to the view. The USERNAME_INDEX property is writable since it
            // maintains username index, currently selected in UI. Updating the property
            // by itself doesn't get propagated to the view as the value originates in the view and
            // gets routed to coordinator through USERNAME_SELECTED_CALLBACK.
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK) {
            assert dialogView instanceof PasswordEditDialogWithDetailsView;
            assert ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS);
            ((PasswordEditDialogWithDetailsView) dialogView)
                    .setUsernameChangedCallback(
                            model.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK));
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK) {
            assert dialogView instanceof UsernameSelectionConfirmationView;
            assert !ChromeFeatureList.isEnabled(
                    ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS);
            ((UsernameSelectionConfirmationView) dialogView)
                    .setUsernameChangedCallback(
                            model.get(PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK));
        } else if (propertyKey == PasswordEditDialogProperties.PASSWORD) {
            dialogView.setPassword(model.get(PasswordEditDialogProperties.PASSWORD));
        } else if (propertyKey == PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK) {
            dialogView.setPasswordChangedCallback(
                    model.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK));
        } else if (propertyKey == PasswordEditDialogProperties.PASSWORD_ERROR) {
            dialogView.setPasswordError(model.get(PasswordEditDialogProperties.PASSWORD_ERROR));
        } else if (propertyKey == PasswordEditDialogProperties.FOOTER) {
            dialogView.setFooter(model.get(PasswordEditDialogProperties.FOOTER));
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    private static void setUsernames(PropertyModel model, PasswordEditDialogView dialogView) {
        if (dialogView instanceof PasswordEditDialogWithDetailsView) {
            assert ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS);
            ((PasswordEditDialogWithDetailsView) dialogView)
                    .setUsernames(model.get(PasswordEditDialogProperties.USERNAMES));
        }
        if (dialogView instanceof UsernameSelectionConfirmationView) {
            assert !ChromeFeatureList.isEnabled(
                    ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS);
            ((UsernameSelectionConfirmationView) dialogView)
                    .setUsernames(model.get(PasswordEditDialogProperties.USERNAMES),
                            model.get(PasswordEditDialogProperties.USERNAME_INDEX));
        }
    }
}
