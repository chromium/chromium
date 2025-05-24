// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.Build;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.Arrays;
import java.util.Collections;
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
public class AccountsReloadingTest {
    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String TEST_EMAIL2 = "test.account2@gmail.com";

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

    private IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager =
                            IdentityServicesProvider.get()
                                    .getIdentityManager(ProfileManager.getLastUsedRegularProfile());
                    mIdentityManager.setRefreshTokenUpdateObserverForTests(mObserver);
                });
    }

    @Test
    @MediumTest
    public void testRefreshTokenUpdateWhenSigninInWithoutSyncWithOneAccountOnDevice() {
        final CoreAccountInfo account1 = mSigninTestRule.addTestAccountThenSignin();

        CriteriaHelper.pollUiThread(() -> mObserver.mCallCount == 1);
        Assert.assertEquals(new HashSet<>(Arrays.asList(account1)), mObserver.mAccountsUpdated);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.R, message = "crbug.com/40816967")
    public void testRefreshTokenUpdateWhenDefaultAccountSignsIn() {
        final CoreAccountInfo account1 = mSigninTestRule.addAccount(TEST_EMAIL1);
        final CoreAccountInfo account2 = mSigninTestRule.addAccount(TEST_EMAIL2);
        CriteriaHelper.pollUiThread(() -> mObserver.mCallCount == 0);
        Assert.assertEquals(Collections.emptySet(), mObserver.mAccountsUpdated);

        SigninTestUtil.signin(account1);

        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 2,
                "Refresh token should only be updated when user signs in. "
                        + "Adding account when user is signed out shouldn't trigger refresh "
                        + "token update.");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(account1, account2)), mObserver.mAccountsUpdated);
    }

    @Test
    @MediumTest
    public void testRefreshTokenUpdateWhenSecondaryAccountSignsIn() {
        final CoreAccountInfo account1 = mSigninTestRule.addAccount(TEST_EMAIL1);
        CriteriaHelper.pollUiThread(() -> mObserver.mCallCount == 0);
        Assert.assertEquals(Collections.emptySet(), mObserver.mAccountsUpdated);

        final CoreAccountInfo account2 = mSigninTestRule.addTestAccountThenSignin();

        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 2,
                "Refresh token should only be updated when user signs in. "
                        + "Adding account when user is signed out shouldn't trigger refresh "
                        + "token update.");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(account1, account2)), mObserver.mAccountsUpdated);
    }

    @Test
    @MediumTest
    public void testRefreshTokenUpdateWhenSignedInUserAddsNewAccount() {
        final CoreAccountInfo account1 = mSigninTestRule.addTestAccountThenSignin();
        CriteriaHelper.pollUiThread(() -> mObserver.mCallCount == 1);
        Assert.assertEquals(new HashSet<>(Arrays.asList(account1)), mObserver.mAccountsUpdated);
        mObserver.mAccountsUpdated.clear();

        final CoreAccountInfo account2 = mSigninTestRule.addAccount(TEST_EMAIL2);

        CriteriaHelper.pollUiThread(
                () -> mObserver.mCallCount == 3,
                "Refresh token should be updated 3 times: "
                        + "1 when user signs in, twice when the signed-in user adds "
                        + "a new account.");
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(account1, account2)), mObserver.mAccountsUpdated);
    }
}
