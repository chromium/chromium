// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.ALL_KEYS;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;

import org.chromium.chrome.browser.password_entry_edit.CredentialEditFragmentView.ComponentStateDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates the credential edit UI and is responsible for managing it.
 */
class CredentialEditCoordinator implements ComponentStateDelegate {
    private final CredentialEditFragmentView mFragmentView;
    private final CredentialEditMediator mMediator;
    private final UiDismissalHandler mDismissalHandler;
    private PropertyModel mModel;

    interface UiDismissalHandler {
        /**
         * Issued when the Ui is being permanently dismissed.
         */
        void onUiDismissed();
    }

    CredentialEditCoordinator(
            CredentialEditFragmentView fragmentView, UiDismissalHandler dismissalHandler) {
        mFragmentView = fragmentView;
        mMediator = new CredentialEditMediator();
        mDismissalHandler = dismissalHandler;
        mFragmentView.setComponentStateDelegate(this);
    }

    void setCredential(String displayUrlOrAppName, String username, String password,
            String displayFederationOrigin) {
        mModel = new PropertyModel.Builder(ALL_KEYS)
                         .with(URL_OR_APP, displayUrlOrAppName)
                         .with(FEDERATION_ORIGIN, displayFederationOrigin)
                         .build();
        CredentialEditCoordinator.setupModelChangeProcessor(mModel, mFragmentView);
        mMediator.initialize(mModel);
        mMediator.setCredential(username, password);
    }

    void dismiss() {
        mMediator.dismiss();
    }

    @Override
    public void onDestroy() {
        mDismissalHandler.onUiDismissed();
    }

    static void setupModelChangeProcessor(PropertyModel model, CredentialEditFragmentView view) {
        PropertyModelChangeProcessor.create(
                model, view, CredentialEditViewBinder::bindCredentialEditView);
    }
}
