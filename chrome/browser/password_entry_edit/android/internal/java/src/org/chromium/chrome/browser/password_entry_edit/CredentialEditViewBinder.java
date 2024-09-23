// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.DUPLICATE_USERNAME_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.EMPTY_PASSWORD_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_ACTION_HANDLER;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Maps {@link CredentialEditProperties} changes in a {@link PropertyModel} to the suitable methods
 * in {@link CredentialEditFragmentView}.
 */
class CredentialEditViewBinder {
    static void bindCredentialEditView(
            PropertyModel model, CredentialEditFragmentView fragmentView, PropertyKey propertyKey) {
        if (propertyKey == UI_ACTION_HANDLER) {
            fragmentView.setUiActionHandler(model.get(UI_ACTION_HANDLER));
        } else if (propertyKey == URL_OR_APP) {
            fragmentView.setUrlOrApp(model.get(URL_OR_APP));
        } else if (propertyKey == FEDERATION_ORIGIN) {
            // TODO(crbug.com/40169863): Treat this case when the federated credentials
            // layout is in place.
        } else if (propertyKey == USERNAME) {
            fragmentView.setUsername(model.get(USERNAME));
        } else if (propertyKey == DUPLICATE_USERNAME_ERROR) {
            fragmentView.changeUsernameError(model.get(DUPLICATE_USERNAME_ERROR));
        } else if (propertyKey == PASSWORD_VISIBLE) {
            fragmentView.changePasswordVisibility(model.get(PASSWORD_VISIBLE));
        } else if (propertyKey == PASSWORD) {
            fragmentView.setPassword(model.get(PASSWORD));
        } else if (propertyKey == EMPTY_PASSWORD_ERROR) {
            fragmentView.changePasswordError(model.get(EMPTY_PASSWORD_ERROR));
        } else if (propertyKey == UI_DISMISSED_BY_NATIVE) {
            fragmentView.dismiss();
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
