// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * This class tests the sign-in checks done at Chrome start-up or when accounts change on device.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(
        reason = "Tests sign-in checks done at Chrome start-up or when accounts change on device.")
public class SigninCheckerTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;

    @After
    public void tearDown() {
        if (mSigninTestRule.getPrimaryAccount() != null) {
            mSigninTestRule.forceSignOut();
        }
    }

    private void initSigninChecker() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        IdentityServicesProvider.get()
                                .getSigninManager(mActivityTestRule.getProfile(false)));
    }

    private int getNumOfChildAccountChecksDone() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninManagerImpl signinManager =
                            (SigninManagerImpl)
                                    IdentityServicesProvider.get()
                                            .getSigninManager(mActivityTestRule.getProfile(false));
                    assert signinManager != null;
                    SigninChecker checker = signinManager.getSigninCheckerForTesting();
                    assert checker != null;
                    return checker.getNumOfChildAccountChecksDoneForTests();
                });
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsTheOnlyAccount() {
        mActivityTestRule.startOnBlankPage();
        initSigninChecker();

        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        CriteriaHelper.pollUiThread(
                () -> {
                    return TestAccounts.CHILD_ACCOUNT.equals(mSigninTestRule.getPrimaryAccount());
                });
        Assert.assertEquals(2, getNumOfChildAccountChecksDone());
    }

    @Test
    @MediumTest
    public void noSigninWhenChildAccountIsTheOnlyAccountButSigninIsNotAllowed() {
        mActivityTestRule.startOnBlankPage();
        initSigninChecker();
        UserActionTester actionTester = new UserActionTester();
        when(mExternalAuthUtilsMock.isGooglePlayServicesMissing(any())).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);

        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        Assert.assertEquals(1, getNumOfChildAccountChecksDone());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount());
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void noSigninWhenChildAccountIsTheSecondaryAccount() {
        // If a child account co-exists with another account on the device, then the child account
        // must be the first device (this is enforced by the Kids Module).  The behaviour in this
        // test case therefore is not currently hittable on a real device; however it is included
        // here for completeness.
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        mActivityTestRule.startOnBlankPage();
        initSigninChecker();
        UserActionTester actionTester = new UserActionTester();

        Assert.assertEquals(1, getNumOfChildAccountChecksDone());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount());
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsFirstAccount() {
        mActivityTestRule.startOnBlankPage();
        initSigninChecker();
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        CriteriaHelper.pollUiThread(
                () -> {
                    return TestAccounts.CHILD_ACCOUNT.equals(mSigninTestRule.getPrimaryAccount());
                });

        // The check should be done twice at account addition and once during force sign-in.
        Assert.assertEquals(3, getNumOfChildAccountChecksDone());
    }
}
