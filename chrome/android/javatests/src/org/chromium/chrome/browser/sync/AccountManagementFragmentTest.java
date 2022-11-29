// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests {@link AccountManagementFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountManagementFragmentTest {
    private static final String CHILD_ACCOUNT_NAME =
            AccountManagerTestRule.generateChildEmail("account@gmail.com");

    public final SettingsActivityTestRule<AccountManagementFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccountManagementFragment.class);

    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.ADD_EDU_ACCOUNT_FROM_ACCOUNT_SETTINGS_FOR_SUPERVISED_USERS})
    public void testAccountManagementFragmentView() throws Exception {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_view");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.ADD_EDU_ACCOUNT_FROM_ACCOUNT_SETTINGS_FOR_SUPERVISED_USERS})
    public void testAccountManagementFragmentViewWithAddEduAccountEnabled() throws Exception {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(
                view, "account_management_fragment_view_with_add_account_for_supervised_users");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testSignedInAccountShownOnTop() throws Exception {
        mSigninTestRule.addAccount("testSecondary@gmail.com");
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_signed_in_account_on_top");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.ADD_EDU_ACCOUNT_FROM_ACCOUNT_SETTINGS_FOR_SUPERVISED_USERS})
    public void testAccountManagementViewForChildAccount() throws Exception {
        mSigninTestRule.addAccountAndWaitForSeeding(CHILD_ACCOUNT_NAME);
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                Profile::getLastUsedRegularProfile);
        CriteriaHelper.pollUiThread(profile::isChild);
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getFragment()
                    .getProfileDataCacheForTesting()
                    .hasProfileDataForTesting(CHILD_ACCOUNT_NAME);
        });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_for_child_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.ADD_EDU_ACCOUNT_FROM_ACCOUNT_SETTINGS_FOR_SUPERVISED_USERS})
    public void testAccountManagementViewForChildAccountWithAddEduAccountEnabled()
            throws Exception {
        mSigninTestRule.addAccountAndWaitForSeeding(CHILD_ACCOUNT_NAME);
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                Profile::getLastUsedRegularProfile);
        CriteriaHelper.pollUiThread(profile::isChild);
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getFragment()
                    .getProfileDataCacheForTesting()
                    .hasProfileDataForTesting(CHILD_ACCOUNT_NAME);
        });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view,
                "account_management_fragment_for_child_account_with_add_account_for_supervised_users");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.ADD_EDU_ACCOUNT_FROM_ACCOUNT_SETTINGS_FOR_SUPERVISED_USERS})
    public void testAccountManagementViewForChildAccountWithSecondaryEduAccount() throws Exception {
        mSigninTestRule.addAccount(CHILD_ACCOUNT_NAME);
        // The code under test doesn't care what account type this is, though in practice only
        // EDU accounts are supported on devices where the primary account is a child account.
        mSigninTestRule.addAccount("account@school.com");
        mSigninTestRule.waitForSeeding();
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                Profile::getLastUsedRegularProfile);
        CriteriaHelper.pollUiThread(profile::isChild);
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getFragment()
                    .getProfileDataCacheForTesting()
                    .hasProfileDataForTesting(CHILD_ACCOUNT_NAME);
        });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_for_child_and_edu_accounts");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.ADD_EDU_ACCOUNT_FROM_ACCOUNT_SETTINGS_FOR_SUPERVISED_USERS})
    public void
    testAccountManagementViewForChildAccountWithSecondaryEduAccountAndAddEduAccountEnabled()
            throws Exception {
        mSigninTestRule.addAccount(CHILD_ACCOUNT_NAME);
        mSigninTestRule.addAccount("account@school.com");
        mSigninTestRule.waitForSeeding();
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                Profile::getLastUsedRegularProfile);
        CriteriaHelper.pollUiThread(profile::isChild);
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getFragment()
                    .getProfileDataCacheForTesting()
                    .hasProfileDataForTesting(CHILD_ACCOUNT_NAME);
        });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view,
                "account_management_fragment_for_child_and_edu_accounts_with_add_account_for_supervised_users");
    }

    @Test
    @SmallTest
    public void testSignOutUserWithoutShowingSignOutDialog() {
        mSigninTestRule.addTestAccountThenSignin();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.sign_out)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse("Account should be signed out!",
                                IdentityServicesProvider.get()
                                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                                        .hasPrimaryAccount(ConsentLevel.SIGNIN)));
    }

    @Test
    @SmallTest
    public void showSignOutDialogBeforeSigningUserOut() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withText(R.string.turn_off_sync_and_signout_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }
}
