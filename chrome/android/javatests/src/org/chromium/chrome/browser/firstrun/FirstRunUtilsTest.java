// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

/**
 * Unit Test for {@link FirstRunUtils}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class FirstRunUtilsTest {
    private FakeAuthenticationAccountManager mAccountManager;
    private AdvancedMockContext mAccountTestingContext;
    private Account mTestAccount;

    public FirstRunUtilsTest() {
        mTestAccount = AccountManagerFacade.createAccountFromName("Dummy");
    }

    @Before
    public void setUp() {
        // GetInstrumentation().getTargetContext() cannot be called in
        // constructor due to external dependencies.
        mAccountTestingContext =
                new AdvancedMockContext(InstrumentationRegistry.getTargetContext());
    }

    private static class FakeAuthenticationAccountManager extends FakeAccountManagerDelegate {
        private final String mAccountType;

        public FakeAuthenticationAccountManager(String accountType) {
            super(FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE);
            mAccountType = accountType;
        }

        @Override
        public AuthenticatorDescription[] getAuthenticatorTypes() {
            AuthenticatorDescription googleAuthenticator =
                    new AuthenticatorDescription(mAccountType, "p1", 0, 0, 0, 0);

            return new AuthenticatorDescription[] {googleAuthenticator};
        }
    }

    private void setUpAccountManager(String accountType) {
        mAccountManager = new FakeAuthenticationAccountManager(accountType);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mAccountManager);
    }

    private void addTestAccount() {
        mAccountManager.addAccountHolderBlocking(
                AccountHolder.builder(mTestAccount).alwaysAccept(true).build());
    }

    // This test previously flaked on the try bot: http://crbug.com/543160.
    // Re-enabling this test since there has been related cleanup/refactoring
    // during the time the test was disabled. If the test starts flaking again,
    // re-open the bug.
    // TODO(nyquist): Remove this if the test is not flaky anymore.
    @Test
    @SmallTest
    @Feature({"FeatureUtilities", "GoogleAccounts"})
    public void testHasGoogleAccountCorrectlyDetected() {
        // Set up an account manager mock that returns Google account types
        // when queried.
        setUpAccountManager(AccountManagerFacade.GOOGLE_ACCOUNT_TYPE);
        addTestAccount();

        ContextUtils.initApplicationContextForTests(mAccountTestingContext);
        boolean hasAccounts = FirstRunUtils.hasGoogleAccounts();

        Assert.assertTrue(hasAccounts);

        boolean hasAuthenticator = FirstRunUtils.hasGoogleAccountAuthenticator();

        Assert.assertTrue(hasAuthenticator);
    }

    // This test previously flaked on the try bot: http://crbug.com/543160.
    // Re-enabling this test since there has been related cleanup/refactoring
    // during the time the test was disabled. If the test starts flaking again,
    // re-open the bug.
    // TODO(nyquist): Remove this if the test is not flaky anymore.
    @Test
    @SmallTest
    @Feature({"FeatureUtilities", "GoogleAccounts"})
    public void testHasNoGoogleAccountCorrectlyDetected() {
        // Set up an account manager mock that doesn't have any accounts and doesn't have Google
        // account authenticator.
        setUpAccountManager("Not A Google Account");

        ContextUtils.initApplicationContextForTests(mAccountTestingContext);
        boolean hasAccounts = FirstRunUtils.hasGoogleAccounts();

        Assert.assertFalse(hasAccounts);

        boolean hasAuthenticator = FirstRunUtils.hasGoogleAccountAuthenticator();

        Assert.assertFalse(hasAuthenticator);
    }
}
