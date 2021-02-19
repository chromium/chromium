// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
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
        if (propertyKey == URL_OR_APP) {
            fragmentView.setUrlOrApp(model.get(URL_OR_APP));
        } else if (propertyKey == FEDERATION_ORIGIN) {
            // TODO(crbug.com/1180252): Treat this case when the federated credentials
            // layout is in place.
        } else if (propertyKey == USERNAME) {
            fragmentView.setUsername(model.get(USERNAME));
        } else if (propertyKey == PASSWORD_VISIBLE) {
            fragmentView.changePasswordVisibility(model.get(PASSWORD_VISIBLE));
        } else if (propertyKey == PASSWORD) {
            if (model.get(FEDERATION_ORIGIN).isEmpty()) {
                fragmentView.setPassword(model.get(PASSWORD));
            }
        } else if (propertyKey == UI_DISMISSED_BY_NATIVE) {
            fragmentView.dismiss();
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
