// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountRenameChecker;
import org.chromium.components.signin.base.CoreAccountInfo;

/**
 * This class tests the sign-in checks done at Chrome start-up or when accounts
 * change on device.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninCheckerTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock
    private AccountRenameChecker.Delegate mAccountRenameCheckerDelegateMock;

    @Before
    public void setUp() {
        AccountRenameChecker.overrideDelegateForTests(mAccountRenameCheckerDelegateMock);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    public void signinWhenPrimaryAccountIsRenamedToAKnownAccount() {
        mAccountManagerTestRule.addAccount("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);
        final CoreAccountInfo expectedPrimaryAccount =
                mAccountManagerTestRule.addAccount(newAccountEmail);

        mAccountManagerTestRule.removeAccount(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return expectedPrimaryAccount.equals(
                    mAccountManagerTestRule.getCurrentSignedInAccount());
        });
    }

    @Test
    @MediumTest
    public void signoutWhenPrimaryAccountIsRenamedToAnUnknownAccount() {
        mAccountManagerTestRule.addAccount("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);

        mAccountManagerTestRule.removeAccount(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount();
        });
        Assert.assertNull(mAccountManagerTestRule.getCurrentSignedInAccount());
    }

    @Test
    @MediumTest
    public void signoutWhenPrimaryAccountIsRemoved() {
        mAccountManagerTestRule.addAccount("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();

        mAccountManagerTestRule.removeAccount(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount();
        });
        Assert.assertNull(mAccountManagerTestRule.getCurrentSignedInAccount());
    }
}
