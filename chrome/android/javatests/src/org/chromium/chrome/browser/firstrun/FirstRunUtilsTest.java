// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.AuthenticatorDescription;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerFacadeImpl;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit Test for {@link FirstRunUtils}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class FirstRunUtilsTest {
    private FakeAuthenticationAccountManager mAccountManager;
    private AdvancedMockContext mAccountTestingContext;

    @BeforeClass
    public static void setUpBeforeClass() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        // GetInstrumentation().getTargetContext() cannot be called in
        // constructor due to external dependencies.
        mAccountTestingContext =
                new AdvancedMockContext(InstrumentationRegistry.getTargetContext());
    }

    @After
    public void tearDown() {
        FirstRunUtils.resetHasGoogleAccountAuthenticator();
        AccountManagerFacadeProvider.setInstanceForTests(null);
    }

    private static class FakeAuthenticationAccountManager extends FakeAccountManagerDelegate {
        private final String mAccountType;

        FakeAuthenticationAccountManager(String accountType) {
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacadeProvider.setInstanceForTests(
                    new AccountManagerFacadeImpl(mAccountManager));
        });
    }

    private void addTestAccount() {
        mAccountManager.addAccount(AccountHolder.createFromEmail("dummy@gmail.com"));
    }

    @Test
    @SmallTest
    @Feature({"GoogleAccounts"})
    public void testHasGoogleAccountCorrectlyDetected() {
        // Set up an account manager mock that returns Google account types
        // when queried.
        setUpAccountManager(AccountUtils.GOOGLE_ACCOUNT_TYPE);
        addTestAccount();

        ContextUtils.initApplicationContextForTests(mAccountTestingContext);
        CriteriaHelper.pollUiThread(FirstRunUtils::hasGoogleAccounts);
        Assert.assertTrue(FirstRunUtils.hasGoogleAccountAuthenticator());
    }

    @Test
    @SmallTest
    @Feature({"GoogleAccounts"})
    public void testHasNoGoogleAccountCorrectlyDetected() {
        // Set up an account manager mock that doesn't have any accounts and doesn't have Google
        // account authenticator.
        setUpAccountManager("Not A Google Account");

        ContextUtils.initApplicationContextForTests(mAccountTestingContext);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(FirstRunUtils.hasGoogleAccounts()); });
        Assert.assertFalse(FirstRunUtils.hasGoogleAccountAuthenticator());
    }
}
