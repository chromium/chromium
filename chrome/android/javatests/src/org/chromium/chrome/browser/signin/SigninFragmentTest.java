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
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.SigninFirstRunFragment;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.DummyUiActivity;

import java.io.IOException;

/**
 * Render tests for signin fragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninFragmentTest {
    /**
     * This class is used to test {@link SigninFirstRunFragment}.
     */
    public static class CustomSigninFirstRunFragment extends SigninFirstRunFragment {
        private FirstRunPageDelegate mFirstRunPageDelegate;

        @Override
        public FirstRunPageDelegate getPageDelegate() {
            return mFirstRunPageDelegate;
        }

        private void setPageDelegate(FirstRunPageDelegate delegate) {
            mFirstRunPageDelegate = delegate;
        }
    }

    @Rule
    public final DisableAnimationsTestRule mNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final SyncTestRule mSyncTestRule = new SyncTestRule();

    @Rule
    public final BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Mock
    private FirstRunPageDelegate mFirstRunPageDelegateMock;

    private SigninActivity mSigninActivity;

    @After
    public void tearDown() throws Exception {
        // Since SigninActivity is launched inside this test class, we need to
        // tear it down inside the class as well.
        if (mSigninActivity != null) {
            ApplicationTestUtils.finishActivity(mSigninActivity);
        }
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNewAccount() throws IOException {
        mSigninActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER);
                });
        mRenderTestRule.render(mSigninActivity.findViewById(R.id.fragment_container),
                "signin_fragment_new_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNotDefaultAccountWithPrimaryAccount() throws IOException {
        CoreAccountInfo accountInfo = mSyncTestRule.addTestAccount();
        mSyncTestRule.addAccount("test.second.account@gmail.com");
        mSigninActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncherImpl.get().launchActivityForPromoChooseAccountFlow(
                            mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER,
                            accountInfo.getEmail());
                });
        mRenderTestRule.render(mSigninActivity.findViewById(R.id.fragment_container),
                "signin_fragment_choose_primary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentNotDefaultAccountWithSecondaryAccount() throws IOException {
        mSyncTestRule.addTestAccount();
        String secondAccountName = "test.second.account@gmail.com";
        mSyncTestRule.addAccount(secondAccountName);
        mSigninActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncherImpl.get().launchActivityForPromoChooseAccountFlow(
                            mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER,
                            secondAccountName);
                });
        mRenderTestRule.render(mSigninActivity.findViewById(R.id.fragment_container),
                "signin_fragment_choose_secondary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo = mSyncTestRule.addTestAccount();
        mSigninActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER,
                            accountInfo.getEmail());
                });
        mRenderTestRule.render(mSigninActivity.findViewById(R.id.fragment_container),
                "signin_fragment_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSigninFragmentForcedSigninWithRegularChild() throws IOException {
        HistogramDelta startPageHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        CoreAccountInfo accountInfo = mSyncTestRule.addTestAccount();
        CustomSigninFirstRunFragment fragment = new CustomSigninFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putString(SigninFirstRunFragment.FORCE_SIGNIN_ACCOUNT_TO, accountInfo.getEmail());
        bundle.putInt(
                SigninFirstRunFragment.CHILD_ACCOUNT_STATUS, ChildAccountStatus.REGULAR_CHILD);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        mActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, fragment)
                    .commit();
        });
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
        Assert.assertEquals(1, startPageHistogram.getDelta());
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_fragment_forced_signin_with_regular_child");
    }

    @Test
    @LargeTest
    public void testClickingSettingsDoesNotSetFirstSetupComplete() {
        CoreAccountInfo accountInfo = mSyncTestRule.addTestAccount();
        mSigninActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mSyncTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            accountInfo.getEmail());
                });
        onView(withText(accountInfo.getEmail())).check(matches(isDisplayed()));
        onView(withId(R.id.signin_details_description)).perform(clickOnClickableSpan());
        // Wait for sign in process to finish.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount();
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertTrue(SyncTestUtil.isSyncRequested());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(ProfileSyncService.get().isFirstSetupComplete()); });
        // Close the SettingsActivity.
        onView(withId(R.id.cancel_button)).perform(click());
    }

    @Test
    @MediumTest
    public void testSigninFragmentWithDefaultFlow() {
        HistogramDelta settingsHistogram =
                new HistogramDelta("Signin.SigninStartedAccessPoint", SigninAccessPoint.SETTINGS);
        mSigninActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncherImpl.get().launchActivity(
                            mSyncTestRule.getActivity(), SigninAccessPoint.SETTINGS);
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
        CoreAccountInfo defaultAccountInfo = mSyncTestRule.addTestAccount();
        String nonDefaultAccountName = "test.account.nondefault@gmail.com";
        mSyncTestRule.addAccount(nonDefaultAccountName);
        mSigninActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SigninActivity.class, () -> {
                    SigninActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mSyncTestRule.getActivity(), SigninAccessPoint.BOOKMARK_MANAGER,
                            defaultAccountInfo.getEmail());
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
}
