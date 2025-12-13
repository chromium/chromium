// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.DataType;

import java.util.Set;

/** Tests {@link AccountManagementFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(https://crbug.com/464015738): these tests could be flaky because of AnimatedProgressBar.
@DisableFeatures({
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER
})
public class AccountManagementFragmentTest {
    private final SyncTestRule mSyncTestRule = new SyncTestRule();
    private static final int RENDER_TEST_REVISION = 2;

    private final SettingsActivityTestRule<AccountManagementFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccountManagementFragment.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work (SyncTestRule extends CTARule).
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    private FakeSyncServiceImpl mFakeSyncService;

    @Before
    public void setUp() {
        mFakeSyncService = overrideSyncService();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAccountManagementFragmentView() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_view");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testSignedInAccountShownOnTop() throws Exception {
        mSyncTestRule.getSigninTestRule().addAccount(TestAccounts.ACCOUNT1);
        mSyncTestRule.getSigninTestRule().addAccountThenSignin(TestAccounts.ACCOUNT2);
        mSettingsActivityTestRule.startSettingsActivity();
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_signed_in_account_on_top");
    }

    @Test
    @MediumTest
    public void testAccountManagementViewForChildAccountWithNonDisplayableAccountEmail()
            throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        AccountInfo accountInfo =
                signinTestRule.addChildTestAccountThenWaitForSignin(
                        new AccountCapabilitiesBuilder().setCanHaveEmailAddressDisplayed(false));
        mSettingsActivityTestRule.startSettingsActivity();

        // Force update the fragment so that NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES is
        // actually utilized. This is to replicate downstream implementation behavior, where
        // checkIfDisplayableEmailAddress() differs.
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .getProfileDataOrDefault(accountInfo.getEmail())
                            .hasDisplayableEmailAddress();
                });
        ThreadUtils.runOnUiThreadBlocking(mSettingsActivityTestRule.getFragment()::update);
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        onView(
                        allOf(
                                isDescendantOfA(withId(android.R.id.list_container)),
                                withText(accountInfo.getFullName())))
                .check(matches(isDisplayed()));
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void
            testAccountManagementViewForChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName()
                    throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        AccountInfo accountInfo = TestAccounts.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME;
        signinTestRule.addAccount(accountInfo);
        // Child accounts are signed-in automatically in the background.
        signinTestRule.waitForSignin(accountInfo);
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .getProfileDataOrDefault(accountInfo.getEmail())
                            .hasDisplayableEmailAddress();
                });
        ThreadUtils.runOnUiThreadBlocking(mSettingsActivityTestRule.getFragment()::update);
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
        onView(
                        allOf(
                                isDescendantOfA(withId(android.R.id.list_container)),
                                withText(R.string.default_google_account_username)))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAccountManagementViewForChildAccount() throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        CoreAccountInfo primarySupervisedAccount =
                signinTestRule.addChildTestAccountThenWaitForSignin();

        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .hasProfileDataForTesting(primarySupervisedAccount.getEmail());
                });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(
                view,
                "account_management_fragment_for_child_account_with_add_account_for_supervised_"
                        + "users");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAccountManagementViewForChildAccountWithSecondaryEduAccount() throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        CoreAccountInfo primarySupervisedAccount =
                signinTestRule.addChildTestAccountThenWaitForSignin();
        // Add a secondary EDU account.
        signinTestRule.addAccount(TestAccounts.ACCOUNT1);
        signinTestRule.waitForSignin(primarySupervisedAccount);

        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .hasProfileDataForTesting(primarySupervisedAccount.getEmail());
                });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(
                view,
                "account_management_fragment_for_child_and_edu_accounts_with_add_account_for_"
                        + "supervised_users");
    }

    @Test
    @SmallTest
    public void testSignOutShowsUnsavedDataDialog() {
        mFakeSyncService.setTypesWithUnsyncedData(Set.of(DataType.BOOKMARKS));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.sign_out)).perform(click());

        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testIdentityErrorCardNotShown() {
        // Fake an identity error.
        mFakeSyncService.setRequiresClientUpgrade(true);

        // Expect no records.
        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Sign in, enable sync and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    private FakeSyncServiceImpl overrideSyncService() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FakeSyncServiceImpl fakeSyncService = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncService);
                    return fakeSyncService;
                });
    }
}
