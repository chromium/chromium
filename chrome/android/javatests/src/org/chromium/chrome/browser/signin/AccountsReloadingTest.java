// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerImpl;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * This class tests the accounts reloading within {@link IdentityManager}.
 *
 * <p>When a user signs in or when a signed in user adds a new accounts, the refresh token should
 * also be updated within {@link IdentityManager}. This is essential for having the accounts in
 * cookie jar and the device accounts consistent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)
public class AccountsReloadingTest {
    private static class Observer implements Callback<CoreAccountInfo> {
        private final Set<CoreAccountInfo> mAccountsUpdated = new HashSet<>();
        private int mCallCount;

        @Override
        public void onResult(CoreAccountInfo coreAccountInfo) {
            mAccountsUpdated.add(coreAccountInfo);
            ++mCallCount;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private final Observer mObserver = new Observer();

    private IdentityManagerImpl mIdentityManager;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager =
                            (IdentityManagerImpl)
                                    IdentityServicesProvider.get()
                                            .getIdentityManager(
                                                    ProfileManager.getLastUsedRegularProfile());
                    mIdentityManager.setRefreshTokenUpdateObserverForTests(mObserver);
                });
    }

    @Test
    @MediumTest
    public void testRefreshTokenUpdateWhenSignInWithOneAccountOnDevice() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 1,
                "Refresh token should be updated once when the account is added");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(TestAccounts.ACCOUNT1)), mObserver.mAccountsUpdated);
    }

    @Test
    @MediumTest
    public void testRefreshTokenUpdateWhenDefaultAccountSignsIn() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);
        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 3,
                "Refresh token should be updated 3 times, once per account");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2)),
                mObserver.mAccountsUpdated);

        SigninTestUtil.signin(TestAccounts.ACCOUNT1);

        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 3, "Refresh token should not be updated on sign in.");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2)),
                mObserver.mAccountsUpdated);
    }

    @Test
    @MediumTest
    public void testRefreshTokenUpdateWhenSecondaryAccountSignsIn() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        CriteriaHelper.pollUiThread(() -> mObserver.mCallCount == 1);
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(TestAccounts.ACCOUNT1)), mObserver.mAccountsUpdated);
        mObserver.mAccountsUpdated.clear();

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT2);

        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 3,
                "Refresh token should be updated 3 times, once per account");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2)),
                mObserver.mAccountsUpdated);
    }

    @Test
    @MediumTest
    public void testRefreshTokenUpdateWhenSignedInUserAddsNewAccount() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 1,
                "Refresh token should be updated once when the account is added");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(TestAccounts.ACCOUNT1)), mObserver.mAccountsUpdated);
        mObserver.mAccountsUpdated.clear();

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 3,
                "Refresh token should be updated 3 times, once per account");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2)),
                mObserver.mAccountsUpdated);
    }
}
