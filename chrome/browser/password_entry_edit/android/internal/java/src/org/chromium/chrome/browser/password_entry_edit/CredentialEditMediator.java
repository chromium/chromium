// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the edit component. It  updates the model when needed and reacts to UI
 * events (e.g. button clicks).
 */
public class CredentialEditMediator {
    private PropertyModel mModel;

    CredentialEditMediator(){};

    void initialize(PropertyModel model) {
        mModel = model;
    }

    void setCredential(String username, String password) {
        mModel.set(USERNAME, username);
        mModel.set(PASSWORD, password);
        // TODO(crbug.com/1175785): Replace the password with the identity provider in the case
        // of federated credentials.
        mModel.set(PASSWORD_VISIBLE, !mModel.get(FEDERATION_ORIGIN).isEmpty());
    }

    void dismiss() {
        mModel.set(UI_DISMISSED_BY_NATIVE, true);
    }
}
