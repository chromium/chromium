// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.DUPLICATE_USERNAME_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.EMPTY_PASSWORD_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.UI_DISMISSED_BY_NATIVE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditCoordinator.CredentialActionDelegate;
import org.chromium.chrome.browser.password_entry_edit.CredentialEditFragmentView.UiActionHandler;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper.ReauthReason;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.widget.Toast;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Contains the logic for the edit component. It  updates the model when needed and reacts to UI
 * events (e.g. button clicks).
 */
public class CredentialEditMediator implements UiActionHandler {
    private final PasswordAccessReauthenticationHelper mReauthenticationHelper;
    private final CredentialActionDelegate mCredentialActionDelegate;
    private PropertyModel mModel;
    private String mOriginalUsername;
    private Set<String> mExistingUsernames;

    CredentialEditMediator(PasswordAccessReauthenticationHelper reauthenticationHelper,
            CredentialActionDelegate credentialActionDelegate) {
        mReauthenticationHelper = reauthenticationHelper;
        mCredentialActionDelegate = credentialActionDelegate;
    };

    void initialize(PropertyModel model) {
        mModel = model;
    }

    void setCredential(String username, String password) {
        mOriginalUsername = username;
        mModel.set(USERNAME, username);
        mModel.set(PASSWORD, password);
        // TODO(crbug.com/1175785): Replace the password with the identity provider in the case
        // of federated credentials.
        mModel.set(PASSWORD_VISIBLE, !mModel.get(FEDERATION_ORIGIN).isEmpty());
    }

    void setExistingUsernames(String[] existingUsernames) {
        mExistingUsernames = new HashSet<>(Arrays.asList(existingUsernames));
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

    @Override
    public void onSave() {
        mCredentialActionDelegate.saveChanges(mModel.get(USERNAME), mModel.get(PASSWORD));
    }

    @Override
    public void onUsernameTextChanged(String username) {
        mModel.set(USERNAME, username);
        mModel.set(DUPLICATE_USERNAME_ERROR,
                !mOriginalUsername.equals(username) && mExistingUsernames.contains(username));
    }

    @Override
    public void onPasswordTextChanged(String password) {
        mModel.set(PASSWORD, password);
        mModel.set(EMPTY_PASSWORD_ERROR, password.isEmpty());
    }

    @Override
    public void onCopyUsername(Context context) {
        copyToClipboard(context, "username", USERNAME);
        Toast.makeText(context, R.string.password_entry_viewer_username_copied_into_clipboard,
                     Toast.LENGTH_SHORT)
                .show();
    }

    @Override
    public void onCopyPassword(Context context) {
        reauthenticateUser(ReauthReason.COPY_PASSWORD, (reauthSucceeded) -> {
            if (!reauthSucceeded) return;
            copyToClipboard(context, "password", PASSWORD);
            Toast.makeText(context, R.string.password_entry_viewer_password_copied_into_clipboard,
                         Toast.LENGTH_SHORT)
                    .show();
        });
    }

    private void reauthenticateUser(@ReauthReason int reason, Callback<Boolean> action) {
        if (!mReauthenticationHelper.canReauthenticate()) {
            mReauthenticationHelper.showScreenLockToast(reason);
            return;
        }
        mReauthenticationHelper.reauthenticate(reason, action);
    }

    private void copyToClipboard(
            Context context, CharSequence label, ReadableObjectPropertyKey<String> dataKey) {
        ClipboardManager clipboard =
                (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText(label, mModel.get(dataKey));
        clipboard.setPrimaryClip(clip);
    }
}
