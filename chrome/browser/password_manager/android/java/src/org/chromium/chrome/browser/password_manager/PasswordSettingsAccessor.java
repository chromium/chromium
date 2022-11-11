// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;

import org.chromium.base.Callback;

import java.util.Optional;

/**
 * Interface for the object mediating access to the password settings.
 */
public interface PasswordSettingsAccessor {
    /**
     * Asynchronously retrieves the value of the "Offer to save passwords" setting.
     *
     * @param account the account from which to retrieve the value or no account if it should be
     *        retrieved from local storage
     * @param successCallback called if the retrieval succeeds with the value of the setting
     *        or no value if none was set
     * @param failureCallback called with an error if the retrieval did not succeed
     */
    default void getOfferToSavePasswords(com.google.common.base.Optional<Account> account,
            Callback<com.google.common.base.Optional<Boolean>> successCallback,
            Callback<Exception> failureCallback) {
        assert false;
    }

    default void getOfferToSavePasswords(Optional<Account> account,
            Callback<Optional<Boolean>> successCallback, Callback<Exception> failureCallback) {
        getOfferToSavePasswords(OptionalUtils.toGuavaOptional(account),
                OptionalUtils.toGuavaOptionalCallback(successCallback), failureCallback);
    }

    /**
     * Asynchronously sets the value of the "Offer to save passwords" setting.
     *
     * @param offerToSavePasswordsEnabled the value to set for the setting.
     * @param account the account from which to retrieve the value or no account if it should be
     *        retrieved from local storage
     * @param successCallback called if the modification was successful
     * @param failureCallback called with an error if the modification did not succeed
     */
    default void setOfferToSavePasswords(boolean offerToSavePasswordsEnabled,
            com.google.common.base.Optional<Account> account, Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
        assert false;
    }

    default void setOfferToSavePasswords(boolean offerToSavePasswordsEnabled,
            Optional<Account> account, Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
        setOfferToSavePasswords(offerToSavePasswordsEnabled, OptionalUtils.toGuavaOptional(account),
                successCallback, failureCallback);
    }

    /**
     * Asynchronously retrieves the value of the "Auto Sign In" setting.
     *
     * @param account the account where to store the value the value or no account if it should be
     *        stored in the local storage
     * @param successCallback called if the retrieval succeeds with the value of the setting
     *        or no value if none was set
     * @param failureCallback called with an error if the retrieval did not succeed
     */
    default void getAutoSignIn(com.google.common.base.Optional<Account> account,
            Callback<com.google.common.base.Optional<Boolean>> successCallback,
            Callback<Exception> failureCallback) {
        assert false;
    }

    default void getAutoSignIn(Optional<Account> account,
            Callback<Optional<Boolean>> successCallback, Callback<Exception> failureCallback) {
        getAutoSignIn(OptionalUtils.toGuavaOptional(account),
                OptionalUtils.toGuavaOptionalCallback(successCallback), failureCallback);
    }

    /**
     * Asynchronously sets the value of the "Auto Sign In" setting.
     *
     * @param autoSignInEnabled the value to set for the setting
     * @param account the account where to store the value the value or no account if it should be
     *        stored in the local storage
     * @param successCallback called if the modification was successful
     * @param failureCallback called with an error if the modification did not succeed
     */
    default void setAutoSignIn(boolean autoSignInEnabled,
            com.google.common.base.Optional<Account> account, Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
        assert false;
    }

    default void setAutoSignIn(boolean autoSignInEnabled, Optional<Account> account,
            Callback<Void> successCallback, Callback<Exception> failureCallback) {
        setAutoSignIn(autoSignInEnabled, OptionalUtils.toGuavaOptional(account), successCallback,
                failureCallback);
    }
}
