// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

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
 * in {@link BlockedCredentialFragmentView}.
 */
public class BlockedCredentialViewBinder {
    static void bindBlockedCredentialView(
            PropertyModel model,
            BlockedCredentialFragmentView fragmentView,
            PropertyKey propertyKey) {
        if (propertyKey == UI_ACTION_HANDLER) {
            fragmentView.setUiActionHandler(model.get(UI_ACTION_HANDLER));
        } else if (propertyKey == URL_OR_APP) {
            fragmentView.setUrlOrApp(model.get(URL_OR_APP));
        } else if (propertyKey == UI_DISMISSED_BY_NATIVE) {
            fragmentView.dismiss();
        } else if (propertyKey == FEDERATION_ORIGIN
                || propertyKey == USERNAME
                || propertyKey == PASSWORD
                || propertyKey == PASSWORD_VISIBLE) {
            // These properties are not relevant for the blocked credential view.
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
