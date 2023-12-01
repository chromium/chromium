// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

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
            dialogView.setUsernames(model.get(PasswordEditDialogProperties.USERNAMES));
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME) {
            dialogView.setUsername(model.get(PasswordEditDialogProperties.USERNAME));
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK) {
            dialogView.setUsernameChangedCallback(
                    model.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK));
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
}
