// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditFragmentView.UiActionHandler;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper.ReauthReason;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the edit component. It  updates the model when needed and reacts to UI
 * events (e.g. button clicks).
 */
public class CredentialEditMediator implements UiActionHandler {
    private final PasswordAccessReauthenticationHelper mReauthenticationHelper;
    private PropertyModel mModel;

    CredentialEditMediator(PasswordAccessReauthenticationHelper reauthenticationHelper) {
        mReauthenticationHelper = reauthenticationHelper;
    };

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

    @Override
    public void onMaskOrUnmaskPassword() {
        if (mModel.get(PASSWORD_VISIBLE)) {
            mModel.set(PASSWORD_VISIBLE, false);
            return;
        }
        reauthenticateUser(ReauthReason.VIEW_PASSWORD, (reauthSucceeded) -> {
            if (!reauthSucceeded) return;

            mModel.set(PASSWORD_VISIBLE, true);
        });
    }

    private void reauthenticateUser(@ReauthReason int reason, Callback<Boolean> action) {
        if (!mReauthenticationHelper.canReauthenticate()) {
            mReauthenticationHelper.showScreenLockToast(reason);
            return;
        }
        mReauthenticationHelper.reauthenticate(reason, action);
    }
}
