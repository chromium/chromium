// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.ALL_KEYS;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Creates the credential edit UI and is responsible for managing it.
 */
class CredentialEditCoordinator {
    private final CredentialEditFragmentView mFragmentView;
    private final CredentialEditMediator mMediator;
    private PropertyModel mModel;

    CredentialEditCoordinator(CredentialEditFragmentView fragmentView) {
        mFragmentView = fragmentView;
        mMediator = new CredentialEditMediator();
    }

    void setCredential(String displayUrlOrAppName, String username, String password,
            String displayFederationOrigin) {
        mModel = new PropertyModel.Builder(ALL_KEYS)
                         .with(URL_OR_APP, displayUrlOrAppName)
                         .with(FEDERATION_ORIGIN, displayFederationOrigin)
                         .build();
        mMediator.initialize(mModel);
        mMediator.setCredential(username, password);
    }

    void dismiss() {
        // TODO(crbug.com/1175785): Dismiss the UI, if it wasn't dismissed already.
    }
}
