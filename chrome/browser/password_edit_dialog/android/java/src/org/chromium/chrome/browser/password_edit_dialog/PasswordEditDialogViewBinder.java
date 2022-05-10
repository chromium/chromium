// Copyright 2022 The Chromium Authors. All rights reserved.
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
            dialogView.setUsernames(model.get(PasswordEditDialogProperties.USERNAMES),
                    model.get(PasswordEditDialogProperties.USERNAME));
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME) {
            // Propagation of USERNAMES property triggers passing both USERNAMES and
            // USERNAME properties to the view. This is safe because both properties
            // are set through property model builder and available by the time the property model
            // is bound to the view. The USERNAME property is writable since it
            // maintains username, currently typed or selected in UI. Updating the property
            // by itself doesn't get propagated to the view as the value originates in the view and
            // gets routed to coordinator through USERNAME_CHANGED_CALLBACK.
        } else if (propertyKey == PasswordEditDialogProperties.FOOTER) {
            dialogView.setFooter(model.get(PasswordEditDialogProperties.FOOTER));
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK) {
            dialogView.setUsernameChangedCallback(
                    model.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK));
        } else if (propertyKey == PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK) {
            dialogView.setPasswordChangedCallback(
                    model.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK));
        } else if (propertyKey == PasswordEditDialogProperties.PASSWORD) {
            dialogView.setPassword(model.get(PasswordEditDialogProperties.PASSWORD));
        } else if (propertyKey == PasswordEditDialogProperties.EMPTY_PASSWORD_ERROR) {
            // TODO(crbug.com/1315916): Handle displaying empty password error in the following CLs
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
