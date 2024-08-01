// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * Test class to verify the {@link SupervisedUserCapabilities} correctly identifies supervised
 * users.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Account needs to be cleared between tests")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SupervisedUserCapabilitiesTest {

    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        mSigninTestRule.forceSignOut();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.REPLACE_PROFILE_IS_CHILD_WITH_ACCOUNT_CAPABILITIES_ON_ANDROID
    })
    public void childAccountWithCapability_isSubjectToParentalControls() {
        AccountInfo accountInfo =
                new AccountInfo.Builder(AccountManagerTestRule.TEST_ACCOUNT_1)
                        .accountCapabilities(
                                new AccountCapabilitiesBuilder()
                                        .setIsSubjectToParentalControls(true)
                                        .build())
                        .build();
        mSigninTestRule.addAccount(accountInfo);
        // Wait for supervised user to be signed in by the SigninChecker.
        mSigninTestRule.waitForSignin(accountInfo);

        // AccountCapabilities will be used to determine child status.
        CriteriaHelper.pollUiThread(
                () -> {
                    return SupervisedUserCapabilities.isSubjectToParentalControls(
                            mActivityTestRule.getProfile(false));
                });
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.REPLACE_PROFILE_IS_CHILD_WITH_ACCOUNT_CAPABILITIES_ON_ANDROID
    })
    public void childAccountWithoutCapability_isSubjectToParentalControls() {
        AccountInfo accountInfo =
                new AccountInfo.Builder(
                                FakeAccountManagerFacade.generateChildEmail("test@gmail.com"),
                                FakeAccountManagerFacade.toGaiaId("test@gmail.com"))
                        .fullName("Test1 Full")
                        .givenName("Test1 Given")
                        .build();
        mSigninTestRule.addAccount(accountInfo);
        // Wait for supervised user to be signed in by the SigninChecker.
        mSigninTestRule.waitForSignin(accountInfo);

        // Profile attributes will be used to determine child status.
        CriteriaHelper.pollUiThread(
                () -> {
                    return SupervisedUserCapabilities.isSubjectToParentalControls(
                            mActivityTestRule.getProfile(false));
                });
    }
}
