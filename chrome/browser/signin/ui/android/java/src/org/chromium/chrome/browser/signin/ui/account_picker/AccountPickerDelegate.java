// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import org.chromium.base.Callback;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;

/**
 * This interface is used in web sign-in flow for the account picker bottom sheet.
 */
public interface AccountPickerDelegate {
    /**
     * Called when the delegate is dismissed to release resources used by this class.
     */
    void onDismiss();

    /**
     * Signs in the user with the given account.
     */
    void signIn(CoreAccountInfo coreAccountInfo,
            Callback<GoogleServiceAuthError> onSignInErrorCallback);

    /**
     * Adds account to device.
     */
    void addAccount(Callback<String> callback);

    /**
     * Updates credentials of the given account name.
     */
    void updateCredentials(String accountName, Callback<Boolean> onUpdateCredentialsCallback);
}
