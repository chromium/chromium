// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.SyncConsentFirstRunFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for sync consent fragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
public class SyncConsentFragmentTest {
    private static final int RENDER_REVISION = 1;
    private static final String RENDER_DESCRIPTION = "Change button style";
    /**
     * This class is used to test {@link SyncConsentFirstRunFragment}.
     */
    public static class CustomSyncConsentFirstRunFragment extends SyncConsentFirstRunFragment {
        private FirstRunPageDelegate mFirstRunPageDelegate;
        private boolean mIsUpdateAccountCalled;

        @Override
        public FirstRunPageDelegate getPageDelegate() {
            return mFirstRunPageDelegate;
        }

        private void setPageDelegate(FirstRunPageDelegate delegate) {
            mFirstRunPageDelegate = delegate;
        }

        @Override
        protected void updateAccounts(List<Account> accounts) {
            super.updateAccounts(accounts);
            mIsUpdateAccountCalled = true;
        }
    }

    @Rule
    public final TestRule mCommandLindFlagRule = CommandLineFlags.getTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_REVISION)
                    .setDescription(RENDER_DESCRIPTION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Mock
    private FirstRunPageDelegate mFirstRunPageDelegateMock;

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    private SyncConsentActivity mSyncConsentActivity;

    @Before
    public void setUp() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        mActivityTestRule.setFinishActivity(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() throws Exception {
        // Since {@link SyncConsentActivity} is launched inside this test class, we need to
        // tear it down inside the class as well.
        if (mSyncConsentActivity != null) {
            ApplicationTestUtils.finishActivity(mSyncConsentActivity);
        }
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNewAccount() throws IOException {
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER);
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signin_fragment_new_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNotDefaultAccountWithPrimaryAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mAccountManagerTestRule.addAccount("test.second.account@gmail.com");
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoChooseAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signin_fragment_choose_primary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNotDefaultAccountWithSecondaryAccount() throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        String secondAccountName = "test.second.account@gmail.com";
        mAccountManagerTestRule.addAccount(secondAccountName);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoChooseAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, secondAccountName);
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signin_fragment_choose_secondary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signin_fragment_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisabledTest(message = "crbug.com/1304737")
    // This test is only relevant if child users do not have sync force-enabled (if they do, then
    // they can only ever access this fragment from the FRE).
    @EnableFeatures({ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS})
    public void testSyncConsentScreenWithChildAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addChildTestAccountThenWaitForSignin();
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_child_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFREFragmentWithNoAccountsOnDevice() throws IOException {
        HistogramDelta startPageHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        Assert.assertEquals(1, startPageHistogram.getDelta());
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_fre_fragment_with_no_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFREFragmentWithAdultAccount() throws IOException {
        HistogramDelta startPageHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        Assert.assertEquals(1, startPageHistogram.getDelta());
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_fre_fragment_with_adult_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentScreenWithChildAccount() throws IOException {
        HistogramDelta startPageHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, true);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        Assert.assertEquals(1, startPageHistogram.getDelta());
        // TODO(https://crbug.com/1291903): Rewrite this test when RenderTestRule is integrated with
        // Espresso.
        // We check the button is enabled rather than visible, as it may be off-screen on small
        // devices.
        onView(withId(R.id.positive_button)).check(matches(isEnabled()));
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "sync_consent_fragment_with_regular_child");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS})
    public void testFRESyncConsentScreenWithChildAccountAllowSyncOff() throws IOException {
        HistogramDelta startPageHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, true);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        Assert.assertEquals(1, startPageHistogram.getDelta());
        // TODO(https://crbug.com/1291903): Rewrite this test when RenderTestRule is integrated with
        // Espresso.
        // We check the button is enabled rather than visible, as it may be off-screen on small
        // devices.
        onView(withId(R.id.positive_button)).check(matches(isEnabled()));
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "sync_consent_fragment_with_regular_child_allow_sync_off");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    public void testFRESyncConsentScreenWithChildAccountLegacy() throws IOException {
        HistogramDelta startPageHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, true);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        Assert.assertEquals(1, startPageHistogram.getDelta());
        // TODO(https://crbug.com/1291903): Rewrite this test when RenderTestRule is integrated with
        // Espresso.
        // We check the button is enabled rather than visible, as it may be off-screen on small
        // devices.
        onView(withId(R.id.positive_button)).check(matches(isEnabled()));
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "sync_consent_fragment_with_regular_child_legacy");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentScreenWhenSignedInWithoutSync() throws IOException {
        mAccountManagerTestRule.addTestAccountThenSignin();
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_when_signed_in_without_sync");
    }

    @Test
    @MediumTest
    public void testFRESyncConsentScreenWhenSelectedAccountIsRemoved() {
        final CoreAccountInfo defaultAccount =
                mAccountManagerTestRule.addAccount("test.default.account@gmail.com");
        final CoreAccountInfo primaryAccount = mAccountManagerTestRule.addTestAccountThenSignin();
        Assert.assertNotEquals(
                "Primary account should be a different account!", defaultAccount, primaryAccount);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);
        launchActivityWithFragment(fragment);

        mAccountManagerTestRule.removeAccount(primaryAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> fragment.mIsUpdateAccountCalled);
        verify(mFirstRunPageDelegateMock).abortFirstRunExperience();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentScreenWhenSignedInWithoutSyncDynamically() throws IOException {
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);
        launchActivityWithFragment(fragment);

        mAccountManagerTestRule.addTestAccountThenSignin();

        CriteriaHelper.pollUiThread(() -> {
            return !mActivityTestRule.getActivity()
                            .findViewById(R.id.signin_account_picker)
                            .isShown();
        });
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_when_signed_in_without_sync_dynamically");
    }

    @Test
    @LargeTest
    public void testClickingSettingsDoesNotSetFirstSetupComplete() {
        CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            accountInfo.getEmail());
                });
        onView(withText(accountInfo.getEmail())).check(matches(isDisplayed()));
        onView(withId(R.id.signin_details_description)).perform(clickOnClickableSpan());
        // Wait for sign in process to finish.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SYNC);
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertTrue(SyncTestUtil.isSyncRequested());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(SyncService.get().isFirstSetupComplete()); });
        // Close the SettingsActivity.
        onView(withId(R.id.cancel_button)).perform(click());
    }

    @Test
    @LargeTest
    public void testSigninFREFragmentWithoutSelectedAccount() {
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);
        launchActivityWithFragment(fragment);

        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);

        onView(withText(R.string.signin_account_picker_dialog_title)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testSigninFragmentWithDefaultFlow() {
        HistogramDelta settingsHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.SETTINGS);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS);
                });
        onView(withId(R.id.positive_button)).check(matches(withText(R.string.signin_add_account)));
        onView(withId(R.id.negative_button)).check(matches(withText(R.string.cancel)));
        Assert.assertEquals(1, settingsHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccountInAccountPickerDialog() {
        HistogramDelta bookmarkHistogram = new HistogramDelta(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.BOOKMARK_MANAGER);
        CoreAccountInfo defaultAccountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        String nonDefaultAccountName = "test.account.nondefault@gmail.com";
        mAccountManagerTestRule.addAccount(nonDefaultAccountName);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, defaultAccountInfo.getEmail());
                });
        onView(withText(defaultAccountInfo.getEmail()))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withText(nonDefaultAccountName)).inRoot(isDialog()).perform(click());
        // We should return to the signin promo screen where the previous account is
        // not shown anymore.
        onView(withText(defaultAccountInfo.getEmail())).check(doesNotExist());
        onView(withText(nonDefaultAccountName)).check(matches(isDisplayed()));
        Assert.assertEquals(1, bookmarkHistogram.getDelta());
    }

    private ViewAction clickOnClickableSpan() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on the one and only clickable span in the view";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length == 0) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                Assert.assertEquals("There should be only one clickable link", 1, spans.length);
                spans[0].onClick(view);
            }
        };
    }

    private void launchActivityWithFragment(Fragment fragment) {
        mActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, fragment)
                    .commit();
        });
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
    }
}
