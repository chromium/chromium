// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccountUtils;

/**
 * Helper class which simulates the GMS Core backend password storage. Should be used for
 * integration tests (like chrome_public_test_apk) interacting with the password store.
 */
public class PasswordManagerTestHelper {

    /** It is not supposed to have any state. */
    private PasswordManagerTestHelper() {}

    /**
     * Fakes the GMS Core backend for password storing. Uses in-memory storage as a password
     * storage. The fake backends are reset automatically after every test (using
     * ResettersForTesting), so it's guaranteed that the password storage would be empty at the
     * beginning of each test. Note that the local password store will be always preset and work by
     * default, but account store needs to be set up preliminary, see `setAccountForPasswordStore`.
     */
    public static void setUpGmsCoreFakeBackends() {
        FakePasswordManagerBackendSupportHelper fakePasswordManagerBackend =
                new FakePasswordManagerBackendSupportHelper();
        fakePasswordManagerBackend.setBackendPresent(true);
        PasswordManagerBackendSupportHelper.setInstanceForTesting(fakePasswordManagerBackend);
        PasswordStoreAndroidBackendFactory.setFactoryInstanceForTesting(
                new FakePasswordStoreAndroidBackendFactoryImpl());
        PasswordSyncControllerDelegateFactory.setFactoryInstanceForTesting(
                new FakePasswordSyncControllerDelegateFactoryImpl());
        PasswordSettingsAccessorFactory.setupFactoryForTesting(
                new FakePasswordSettingsAccessorFactoryImpl());
    }

    /**
     * Sets the account password store. It's a requirement to first call this method before
     * adding/updating/removing passwords for the account (with the exception of using null Account,
     * this will update the local password store).
     *
     * @param email The email of the syncing account used for testing
     */
    public static void setAccountForPasswordStore(String email) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((FakePasswordStoreAndroidBackend)
                                    PasswordStoreAndroidBackendFactory.getInstance()
                                            .createBackend())
                            .setSyncingAccount(AccountUtils.createAccountFromName(email));
                });
    }
}
