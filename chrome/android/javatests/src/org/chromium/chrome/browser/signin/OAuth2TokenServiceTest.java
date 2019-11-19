// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.OAuth2TokenService;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.AccountManagerTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.concurrent.CountDownLatch;

/** Tests for OAuth2TokenService. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class OAuth2TokenServiceTest {
    private AdvancedMockContext mContext;

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();
    private OAuth2TokenService mOAuth2TokenService;

    /**
     * Class handling GetAccessToken callbacks and providing a blocking {@link
     * #getToken()}.
     */
    class GetAccessTokenCallbackForTest implements OAuth2TokenService.GetAccessTokenCallback {
        private String mToken;
        final CountDownLatch mTokenRetrievedCountDown = new CountDownLatch(1);

        /**
         * Blocks until the callback is called once and returns the token.
         * See {@link CountDownLatch#await}
         */
        public String getToken() {
            try {
                mTokenRetrievedCountDown.await();
            } catch (InterruptedException e) {
                throw new RuntimeException("Interrupted or timed-out while waiting for updates", e);
            }
            return mToken;
        }

        @Override
        public void onGetTokenSuccess(String token) {
            mToken = token;
            mTokenRetrievedCountDown.countDown();
        }

        @Override
        public void onGetTokenFailure(boolean isTransientError) {
            mToken = null;
            mTokenRetrievedCountDown.countDown();
        }
    }

    @Before
    public void setUp() {
        mContext = new AdvancedMockContext(InstrumentationRegistry.getTargetContext());
        mOAuth2TokenService = new OAuth2TokenService(0 /*nativeOAuth2TokenServiceDelegate*/,
                null /* AccountTrackerService */, AccountManagerFacade.get());
    }

    @After
    public void tearDown() {
        AccountManagerFacade.resetAccountManagerFacadeForTests();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetAccountsNoAccountsRegistered() {
        String[] accounts = OAuth2TokenService.getAccounts();
        Assert.assertEquals("There should be no accounts registered", 0, accounts.length);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetAccountsOneAccountRegistered() {
        Account account1 = AccountManagerFacade.createAccountFromName("foo@gmail.com");
        AccountHolder accountHolder1 = AccountHolder.builder(account1).build();
        mAccountManagerTestRule.addAccount(accountHolder1);

        String[] sysAccounts = mOAuth2TokenService.getSystemAccountNames();
        Assert.assertEquals("There should be one registered account", 1, sysAccounts.length);
        Assert.assertEquals("The account should be " + account1, account1.name, sysAccounts[0]);

        String[] accounts = OAuth2TokenService.getAccounts();
        Assert.assertEquals("There should be zero registered account", 0, accounts.length);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetAccountsTwoAccountsRegistered() {
        Account account1 = AccountManagerFacade.createAccountFromName("foo@gmail.com");
        AccountHolder accountHolder1 = AccountHolder.builder(account1).build();
        mAccountManagerTestRule.addAccount(accountHolder1);
        Account account2 = AccountManagerFacade.createAccountFromName("bar@gmail.com");
        AccountHolder accountHolder2 = AccountHolder.builder(account2).build();
        mAccountManagerTestRule.addAccount(accountHolder2);

        String[] sysAccounts = mOAuth2TokenService.getSystemAccountNames();
        Assert.assertEquals("There should be one registered account", 2, sysAccounts.length);
        Assert.assertTrue("The list should contain " + account1,
                Arrays.asList(sysAccounts).contains(account1.name));
        Assert.assertTrue("The list should contain " + account2,
                Arrays.asList(sysAccounts).contains(account2.name));

        String[] accounts = OAuth2TokenService.getAccounts();
        Assert.assertEquals("There should be zero registered account", 0, accounts.length);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetOAuth2AccessTokenWithTimeoutOnSuccess() {
        String authToken = "someToken";
        // Auth token should be successfully received.
        runTestOfGetOAuth2AccessTokenWithTimeout(authToken);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetOAuth2AccessTokenWithTimeoutOnError() {
        String authToken = null;
        // Should not crash when auth token is null.
        runTestOfGetOAuth2AccessTokenWithTimeout(authToken);
    }

    private void runTestOfGetOAuth2AccessTokenWithTimeout(String expectedToken) {
        String scope = "oauth2:http://example.com/scope";
        Account account = AccountManagerFacade.createAccountFromName("test@gmail.com");

        // Add an account with given auth token for the given scope, already accepted auth popup.
        AccountHolder accountHolder = AccountHolder.builder(account)
                                              .hasBeenAccepted(scope, true)
                                              .authToken(scope, expectedToken)
                                              .build();
        mAccountManagerTestRule.addAccount(accountHolder);
        GetAccessTokenCallbackForTest callback = new GetAccessTokenCallbackForTest();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mOAuth2TokenService.getAccessToken(account, scope, callback); });
        Assert.assertEquals(expectedToken, callback.getToken());
    }
}
