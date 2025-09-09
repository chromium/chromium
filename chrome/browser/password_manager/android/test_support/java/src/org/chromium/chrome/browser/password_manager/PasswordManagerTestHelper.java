// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.ThreadUtils;
import org.chromium.components.password_manager.AndroidRequirements;
import org.chromium.components.signin.AccountUtils;

/**
 * Helper class which simulates the GMS Core backend password storage. Should be used for
 * integration tests (like chrome_public_test_apk) interacting with the password store.
 */
public class PasswordManagerTestHelper {
    /** It is not supposed to have any state. */
    private PasswordManagerTestHelper() {}

    /**
     * Sets a fake GMS Core version equal to the minimum required one by the password manger. In
     * order to be able to use the PasswordStore, this has to be set before the ProfileStoreFactory
     * creates the store, so either before native initialization or very early in the process.
     *
     * @deprecated If using the password store directly, skip the test on incompatible
     *     configurations with GmsCoreVersionRestriction. If the password store code is exercised
     *     indirectly on a device with outdated GMS Core version, check that it's not a bug and
     *     avoid it. Production code should never try to write to the password store if the GMS Core
     *     version is too old.
     */
    @Deprecated
    public static void setUpPwmRequiredMinGmsVersion() {
        boolean hasInternalBackend = AndroidRequirements.get().hasInternalBackend();
        AndroidRequirements.setForTesting(
                new AndroidRequirements(/* hasMinGmsVersion= */ true, hasInternalBackend));
    }

    /**
     * Fakes the GMS Core backend for password storing. Uses in-memory storage as a password
     * storage. The fake backends are reset automatically after every test (using
     * ResettersForTesting), so it's guaranteed that the password storage would be empty at the
     * beginning of each test. Note that the local password store will be always preset and work by
     * default, but account store needs to be set up preliminary, see `setAccountForPasswordStore`.
     */
    public static void setUpGmsCoreFakeBackends() {
        boolean hasMinGmsVersion = AndroidRequirements.get().hasMinGmsVersion();
        AndroidRequirements.setForTesting(
                new AndroidRequirements(hasMinGmsVersion, /* hasInternalBackend= */ true));
        CredentialManagerLauncherFactory.setFactoryForTesting(
                new FakeCredentialManagerLauncherFactoryImpl());
        PasswordCheckupClientHelperFactory.setFactoryForTesting(
                new FakePasswordCheckupClientHelperFactoryImpl());
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
                            .setSyncingAccount(AccountUtils.createAccountFromEmail(email));
                });
    }
}
