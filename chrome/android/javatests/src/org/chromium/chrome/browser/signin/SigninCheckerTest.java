// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountRenameChecker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * This class tests the sign-in checks done at Chrome start-up or when accounts change on device.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninCheckerTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock private AccountRenameChecker.Delegate mAccountRenameCheckerDelegateMock;

    @Before
    public void setUp() {
        AccountRenameChecker.overrideDelegateForTests(mAccountRenameCheckerDelegateMock);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signinWhenPrimaryAccountIsRenamedToAKnownAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);
        final CoreAccountInfo expectedPrimaryAccount = mSigninTestRule.addAccount(newAccountEmail);

        mSigninTestRule.removeAccount(oldAccount.getId());

        CriteriaHelper.pollUiThread(
                () -> {
                    return expectedPrimaryAccount.equals(
                            mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
                });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountIsRenamedToAnUnknownAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);

        mSigninTestRule.removeAccount(oldAccount.getId());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountIsRemoved() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        mSigninTestRule.removeAccount(oldAccount.getId());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountWithoutSyncConsentIsRemoved() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSignin();

        mSigninTestRule.removeAccount(oldAccount.getId());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsTheOnlyAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        CriteriaHelper.pollUiThread(
                () -> {
                    return AccountManagerTestRule.TEST_CHILD_ACCOUNT.equals(
                            mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
                });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertEquals(
                2,
                SigninCheckerProvider.get(mActivityTestRule.getProfile(false))
                        .getNumOfChildAccountChecksDoneForTests());
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            // When REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS is enabled - the data is no longer wiped
            // on a supervised sign-in.
            Assert.assertTrue(
                    actionTester
                            .getActions()
                            .contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
        }
        Assert.assertFalse(SyncTestUtil.isSyncFeatureEnabled());
    }

    @Test
    @MediumTest
    public void noSigninWhenChildAccountIsTheOnlyAccountButSigninIsNotAllowed() {
        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();
        when(mExternalAuthUtilsMock.isGooglePlayServicesMissing(any())).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);

        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        Assert.assertEquals(
                1,
                SigninCheckerProvider.get(mActivityTestRule.getProfile(false))
                        .getNumOfChildAccountChecksDoneForTests());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
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
        mSigninTestRule.addAccount("the.default.account@gmail.com");
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        Assert.assertEquals(
                0,
                SigninCheckerProvider.get(mActivityTestRule.getProfile(false))
                        .getNumOfChildAccountChecksDoneForTests());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsFirstAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        mSigninTestRule.addAccount("the.second.account@gmail.com");

        UserActionTester actionTester = new UserActionTester();

        CriteriaHelper.pollUiThread(
                () -> {
                    return AccountManagerTestRule.TEST_CHILD_ACCOUNT.equals(
                            mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
                });

        // The check should be done twice at account addition and once during force sign-in.
        Assert.assertEquals(
                3,
                SigninCheckerProvider.get(mActivityTestRule.getProfile(false))
                        .getNumOfChildAccountChecksDoneForTests());
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            // When REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS is enabled - the data is no longer wiped
            // on a supervised sign-in.
            Assert.assertTrue(
                    actionTester
                            .getActions()
                            .contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
        }
    }
}
