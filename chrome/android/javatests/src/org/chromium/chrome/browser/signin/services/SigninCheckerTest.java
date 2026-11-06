// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.model.Statement;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.EnableLeakChecks;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * This class tests the sign-in checks done at Chrome start-up or when accounts change on device.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableLeakChecks
public class SigninCheckerTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private final SigninTestRule mSigninTestRule = SigninTestRule.createWithCleanups();

    // For @RequiresRestart tests (which run in an isolated process invocation outside the
    // PER_CLASS batch), skip SigninTestRule.tearDownRule() so cleanUpAccountsAndSignOut() does not
    // remove the account's refresh token and destroy the pending native AccountManagedStatusFinder
    // before LeakCanaryChecker.onAfterTestClass() inspects the heap.
    @Rule(order = 0)
    public final TestRule mSigninTestRuleWrapper =
            (base, description) -> {
                if (description.getAnnotation(RequiresRestart.class) != null) {
                    return new Statement() {
                        @Override
                        public void evaluate() throws Throwable {
                            mSigninTestRule.setUpRule();
                            base.evaluate();
                        }
                    };
                }
                return mSigninTestRule.apply(base, description);
            };

    @Rule(order = 1)
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> getSigninChecker().resetForTesting());
    }

    private SigninChecker getSigninChecker() {
        SigninManagerImpl signinManager =
                (SigninManagerImpl)
                        IdentityServicesProvider.get()
                                .getSigninManager(mActivityTestRule.getProfile(false));
        assert signinManager != null;
        SigninChecker checker = signinManager.getSigninCheckerForTesting();
        assert checker != null;
        return checker;
    }

    private int getNumOfChildAccountChecksDone() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> getSigninChecker().getNumOfChildAccountChecksDoneForTests());
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsTheOnlyAccount() {
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
        UserActionTester actionTester = new UserActionTester();

        Assert.assertEquals(2, getNumOfChildAccountChecksDone());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount());
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsFirstAccount() {
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        CriteriaHelper.pollUiThread(
                () -> {
                    return TestAccounts.CHILD_ACCOUNT.equals(mSigninTestRule.getPrimaryAccount());
                });

        // The check should be done twice at account addition and once during force sign-in.
        Assert.assertEquals(3, getNumOfChildAccountChecksDone());
    }

    @Test
    @MediumTest
    @RequiresRestart("Leaves AccountManagedStatusFinder pending until after-class LeakCanary runs")
    @EnableFeatures({ChromeFeatureList.VERIFY_STARTUP_SIGNIN_STATE})
    public void startupSigninStateCheckDoesNotLeakActivityWhenPending() {
        mActivityTestRule.closeAllWindowsAndDeleteInstanceAndTabState();
        TrafficControl.hopOffPublicTransit();
        AccountInfo pendingManagedAccount =
                new AccountInfo.Builder(
                                "user@example.com",
                                FakeAccountManagerFacade.toGaiaId("user@example.com"))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        IdentityServicesProvider.get()
                                .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                                .setUserAcceptedAccountManagement(true));
        mSigninTestRule.addAccountThenSignin(pendingManagedAccount);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        IdentityServicesProvider.get()
                                .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                                .setUserAcceptedAccountManagement(false));
        mActivityTestRule.startOnBlankPage();
    }
}
