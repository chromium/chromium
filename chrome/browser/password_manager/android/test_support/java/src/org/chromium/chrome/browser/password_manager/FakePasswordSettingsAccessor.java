// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;

import com.google.common.base.Optional;

import org.chromium.base.Callback;

/**
 * Fake {@link PasswordSettingsAccessor} to be used in integration tests.
 */
public class FakePasswordSettingsAccessor implements PasswordSettingsAccessor {
    @Override
    public void getOfferToSavePasswords(Optional<Account> account,
            Callback<Optional<Boolean>> successCallback, Callback<Exception> failureCallback) {
        // TODO(crbug/1336641): Implement the method of the fake accessor.
    }

    @Override
    public void setOfferToSavePasswords(boolean offerToSavePasswordsEnabled,
            Optional<Account> account, Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
        // TODO(crbug/1336641): Implement the method of the fake accessor.
    }

    @Override
    public void getAutoSignIn(Optional<Account> account,
            Callback<Optional<Boolean>> successCallback, Callback<Exception> failureCallback) {
        // TODO(crbug/1336641): Implement the method of the fake accessor.
    }

    @Override
    public void setAutoSignIn(boolean autoSignInEnabled, Optional<Account> account,
            Callback<Void> successCallback, Callback<Exception> failureCallback) {
        // TODO(crbug/1336641): Implement the method of the fake accessor.
    }
}
